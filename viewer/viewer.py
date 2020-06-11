import os
import sys
import zmq
import time
import argparse
from pxr import Usd, Sdf

def get_layer_file_path(layer_path):
    if layer_path == '/':
        layer_path = '/root'
    return '.{}.usda'.format(layer_path)

def get_layer_save_path(layer_path):
    return os.path.join('tmp_scene', get_layer_file_path(layer_path))


class Layer:
    def __init__(self, timestamp, data):
        self.timestamp = timestamp
        self.data = data

    def __repr__(self):
        return 'Layer(timestamp={}, data={})'.format(self.timestamp, self.data)

    def save(self, path):
        save_path = get_layer_save_path(path)
        save_dir = os.path.dirname(save_path)
        os.makedirs(save_dir, exist_ok=True)
        with open(save_path, 'wb') as file:
            file.write(self.data)

class DataModel:
    def __init__(self):
        self.root_stage = Usd.Stage.CreateNew(get_layer_save_path('/'))
        self.layers = {}
        self.is_layers_dirty = True

    def add_layer(self, path, layer):
        cached_layer = self.get_layer(path)
        if cached_layer:
            if cached_layer.timestamp >= layer.timestamp:
                print('Viewer: discard "{}" as outdated'.format(path))
                return

            print('Viewer: edit "{}"'.format(layer))
            self.layers[path] = layer
        else:
            print('Viewer: new "{}"'.format(layer))
            self.layers[path] = layer
        self.is_layers_dirty = True

        layer.save(path)

    def remove_layer(self, path):
        layer = self.layers.pop(path, None)
        if layer:
            print('Viewer: remove layer "{}"'.format(path))
            self.is_layers_dirty = True

            try:
                os.remove(get_layer_save_path(path))
            except:
                print('Viewer: failed to remove "{}" layer file'.format(path))
        else:
            print('Viewer: failed to remove layer - "{}" does not exist'.format(path))

    def get_layer(self, path):
        return self.layers.get(path, None)


class NetworkController:
    command_layer = 'layer'
    command_layer_edit = 'layerEdit'
    command_get_stage = 'getStage'

    def __init__(self, parser_data, data_model):
        self.data_model = data_model

        self.zmqCtx = zmq.Context(1)
        self.control_socket_add = parser_data.control
        self.control_socket = self.zmqCtx.socket(zmq.REQ)
        self.control_socket.connect(self.control_socket_add)
        print('Viewer: control_socket connected')

        self.notify_socket = self.zmqCtx.socket(zmq.PULL)
        self.notify_socket.bind('tcp://127.0.0.1:*')

        reply = self._try_request(self.compose_notify_socket_init_message)
        if not reply:
            raise Exception('Failed to initialize notify socket: plugin is offline')

        reply_string = reply.decode('utf-8')
        if reply_string != 'ok':
            # How can we handle it in another way?
            raise Exception('Failed to initialize notify socket: {}'.format(reply_string))

    def compose_notify_socket_init_message(self, socket):
        socket.send_string('initNotifySocket', zmq.SNDMORE)
        addr = self.notify_socket.getsockopt_string(zmq.LAST_ENDPOINT)
        print('Viewer: sending initNotifySocket command: {}'.format(addr))
        socket.send_string(addr)
        print('Viewer: initNotifySocket sent')

    def update(self):
        # self._try_request(lambda socket: socket.send_string('ping'), 1, 100)
        while True:
            timeout = 0 if self.data_model.layers else -1
            if self.notify_socket.poll(timeout) != zmq.POLLIN:
                break

            print('Viewer: receiving from notify socket')
            messages = self.notify_socket.recv_multipart()
            command = messages[0].decode('utf-8')
            print('Viewer: received command - {}'.format(command))

            if command == NetworkController.command_layer:
                # Expected three more messages: layer_path, timestamp and layer itself
                if len(messages) == 4:
                    layer_path = messages[1].decode('utf-8')
                    timestamp = int(messages[2])

                    self.data_model.add_layer(layer_path, Layer(timestamp, messages[3]))

            elif command == NetworkController.command_layer_edit:
                # Expected two more messages: layer_path and timestamp
                if len(messages) == 3:
                    layer_path = messages[1].decode('utf-8')
                    timestamp = int(messages[2])

                    if timestamp == 0: # layer has been removed
                        self.data_model.remove_layer(layer_path)
                    else:
                        cachedLayer = self.data_model.get_layer(layer_path)
                        if cachedLayer and cachedLayer.timestamp >= timestamp:
                            print('Viewer: discard "{}" as outdated'.format(layer_path))
                            continue

                        # Here we need to decide if we need this layer immediately
                        # For example, user can disable part of scene,
                        # so there is no reason to load disabled layer
                        #
                        # We do not have such functionality yet, so make a request
                        request_command = 'getLayer ' + layer_path
                        self._try_request(lambda socket: socket.send_string(request_command))

        if data_model.is_layers_dirty:
            data_model.is_layers_dirty = False

            with Sdf.ChangeBlock():
                sublayers = data_model.root_stage.GetRootLayer().subLayerPaths
                # TODO: remove or add only those affected layers
                sublayers.clear()
                for path, layer in data_model.layers.items():
                    sublayers.append(get_layer_file_path(path))
                data_model.root_stage.subLayerPaths = sublayers
            data_model.root_stage.Save()

    def request_stage(self):
        self._try_request(lambda socket: socket.send_string(NetworkController.command_get_stage))

    def _try_request(self, messageComposer, num_retries=3, request_timeout=2500):
        for i in range(num_retries):
            messageComposer(self.control_socket)

            if self.control_socket.poll(request_timeout) == zmq.POLLIN:
                return self.control_socket.recv(copy=True)
            else:
                print('Viewer: no response from plugin, retrying...')
                addr = self.control_socket.getsockopt_string(zmq.LAST_ENDPOINT)
                self.control_socket.setsockopt(zmq.LINGER, 0)
                self.control_socket.close()

                if i + 1 != num_retries:
                    self.control_socket = self.zmqCtx.socket(zmq.REQ)
                    self.control_socket.connect(self.control_socket_add)
                else:
                    raise Exception('Viewer: plugin is offline')


def render(network_controller):
    while True:
        network_controller.update()
        if network_controller.data_model.layers:
            # Actual render
            time.sleep(1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--control', type=str, required=True)
    args = parser.parse_args()

    print('Viewer: pwd="{}"'.format(os.getcwd()))
    print('Viewer: control="{}"'.format(args.control))

    network_controller = NetworkController(args, DataModel())
    network_controller.request_stage()
    render(network_controller)
