import sys
import zmq
import time
import argparse

class DataModel:
    def __init__(self):
        self.stage = None
        self.stage_timestamp = 0

class NetworkController:
    command_get_stage = 'getStage'

    def __init__(self, parser_data, data_model):
        self.data_model = data_model

        self.zmqCtx = zmq.Context(1)
        self.control_socket = self.zmqCtx.socket(zmq.REQ)
        self.control_socket.connect(parser_data.control)
        print('Viewer: control_socket connected')

        self.notify_socket = self.zmqCtx.socket(zmq.REP)
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
            timeout = 0 if self.data_model.stage else -1
            if self.notify_socket.poll(timeout) != zmq.POLLIN:
                break

            print('Viewer: receiving from notify socket')
            messages = self.notify_socket.recv_multipart()
            command = messages[0].decode('utf-8')
            print('Viewer: received command - {}'.format(command))

            if command == NetworkController.command_get_stage:
                # Expected two more messages: timestamp and stage itself
                if len(messages) == 3:
                    timestamp = int(messages[1])
                    if timestamp > self.data_model.stage_timestamp:
                        self.data_model.stage_timestamp = timestamp
                        self.data_model.stage = messages[2]
                        print('Viewer: new stage with timestamp {}'.format(timestamp))
                        print(self.data_model.stage)
                    else:
                        print('Viewer: discard stage as outdated')

                    self.notify_socket.send_string('ok')
                else:
                    self.notify_socket.send_string('incorrect message structure')
                continue

            self.notify_socket.send_string('fail')

    def request_stage(self):
        self._try_request(lambda socket: socket.send_string(command_get_stage), 1, 0)

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
                    self.control_socket.connect(parser_data.control)
                else:
                    print('Viewer: plugin is offline');

        return None

def render(network_controller):
    while True:
        network_controller.update()
        if network_controller.data_model.stage:
            # Actual render
            time.sleep(1)
        else:
            network_controller.request_stage()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--control', type=str, required=True)
    args = parser.parse_args()

    print('Viewer: control="{}"'.format(args.control))

    network_controller = NetworkController(args, DataModel())
    render(network_controller)
