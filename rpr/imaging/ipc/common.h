#ifndef RPR_IMAGING_IPC_COMMON_H
#define RPR_IMAGING_IPC_COMMON_H

#include <zmq.hpp>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

template <typename U>
struct is_buffer_like {
private:
    template<typename T>
    static constexpr auto check(T*) -> typename
        std::is_pointer<decltype(std::declval<T>().data())>::type;

    template<typename>
    static constexpr std::false_type check(...);

    typedef decltype(check<U>(0)) type;

public:
    static constexpr bool value = type::value;
};
template< class T >
constexpr bool is_buffer_like_v = is_buffer_like<T>::value;

template <typename T>
std::enable_if_t<is_buffer_like_v<T>, zmq::message_t>
GetZmqMessage(T const& command) {
    return {command.data(), command.size()};
}

template <typename T>
std::enable_if_t<std::is_integral_v<T>, zmq::message_t>
GetZmqMessage(T integer) {
    return GetZmqMessage(std::to_string(integer));
}

inline std::string GetSocketAddress(zmq::socket_t const& socket) {
    return socket.get(zmq::sockopt::last_endpoint);
}

inline std::string GetSocketPort(zmq::socket_t const& socket) {
    auto addr = GetSocketAddress(socket);
    return addr.substr(addr.find_last_of(':') + 1);
}

inline void ProxyMessages(zmq::message_t& lastMessage, zmq::socket_t& from, zmq::socket_t& to) {
    bool moreMessages;
    do {
        moreMessages = lastMessage.more();

        if (to) {
            auto sendFlags = moreMessages ? zmq::send_flags::sndmore : zmq::send_flags::none;
            to.send(lastMessage, sendFlags);
        }

        if (moreMessages) {
            from.recv(lastMessage);
        }
    } while (moreMessages);
}

inline zmq::recv_result_t RecvMore(zmq::socket_t& socket, zmq::message_t& msg, zmq::recv_flags flags = zmq::recv_flags::none) {
    if (!msg.more()) {
        throw std::runtime_error("Invalid message sequence");
    }
    return socket.recv(msg, flags);
}

inline void DropRemainingMessages(zmq::socket_t& socket, zmq::message_t& lastMessage) {
    bool moreMessages;
    do {
        moreMessages = lastMessage.more();
        if (moreMessages) {
            socket.recv(lastMessage);
        }
    } while (moreMessages);
}

PXR_NAMESPACE_CLOSE_SCOPE

namespace std {

inline std::string to_string(zmq::message_t& msg) {
    return std::string(msg.data<char>(), msg.size());
}

};

#endif // RPR_IMAGING_IPC_COMMON_H
