import os
import sys
import zmq
import time
import argparse

def get_layer_save_path(layer_path):
    if layer_path == '/':
        layer_path = '/root'
    return '.{}.usda'.format(layer_path)

def save_layer(layer_path, layer):
    save_path = get_layer_save_path(layer_path)
    save_dir = os.path.dirname(save_path)
    os.makedirs(save_dir, exist_ok=True)
    with open(save_path, 'wb') as file:
        file.write(layer)

class Layer:
    def __init__(self, timestamp, data):
        self.timestamp = timestamp
        self.data = data

class DataModel:
    def __init__(self):
        self.layers = {}

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

                    cachedLayer = self.data_model.layers.get(layer_path, None)
                    if cachedLayer and cachedLayer.timestamp >= timestamp:
                        print('Viewer: discard "{}" as outdated'.format(layer_path))
                        continue

                    self.data_model.layers[layer_path] = Layer(timestamp, messages[3])

                    print('Viewer: new layer "{}" with timestamp {}'.format(layer_path, timestamp))
                    save_layer(layer_path, messages[3])

            elif command == NetworkController.command_layer_edit:
                # Expected two more messages: layer_path and timestamp
                if len(messages) == 3:
                    layer_path = messages[1].decode('utf-8')
                    timestamp = int(messages[2])

                    if timestamp == 0: # layer has been removed
                        try:
                            os.remove(get_layer_save_path(layer_path))
                            print('Viewer: removed "{}" layer'.format(layer_path))
                        except:
                            print('Viewer: failed to remove "{}" layer'.format(layer_path))
                    else:
                        cachedLayer = self.data_model.layers.get(layer_path, None)
                        if cachedLayer and cachedLayer.timestamp >= timestamp:
                            print('Viewer: discard "{}" as outdated'.format(layer_path))
                            continue

                        print('Viewer: edited layer: "{}"'.format(layer_path))
                        # Here we need to decide if we need this layer immediately
                        # For example, user can disable part of scene,
                        # so there is no reason to load disabled layer
                        # We do not have such functionality yet, so make a request
                        request_command = 'getLayer ' + layer_path
                        self._try_request(lambda socket: socket.send_string(request_command))

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
