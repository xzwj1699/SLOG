#include "sender.h"

using std::move;

namespace slog {

// Must start from 1 because identities starting with 0 are reserved for ZMQ
std::atomic<uint8_t> Sender::counter(1);

Sender::Sender(const std::shared_ptr<Broker>& broker)
    : context_(broker->context()), broker_(broker), local_machine_id_(broker->local_machine_id()) {}

void Sender::SendSerialized(const internal::Envelope& envelope, MachineId to_machine_id, Channel to_channel) {
  // Lazily establish a new connection when necessary
  auto it = machine_id_to_socket_.find(to_machine_id);
  if (it == machine_id_to_socket_.end()) {
    if (auto br = broker_.lock()) {
      zmq::socket_t new_socket(*context_, ZMQ_PUSH);
      new_socket.set(zmq::sockopt::sndhwm, 0);
      new_socket.connect(br->GetEndpointByMachineId(to_machine_id));
      auto res = machine_id_to_socket_.insert_or_assign(to_machine_id, move(new_socket));
      it = res.first;
    } else {
      // Broker has been destroyed. This can only happen during cleaning up
      return;
    }
  }

  SendSerializedProto(it->second, envelope, local_machine_id_, to_channel);
}

void Sender::SendLocal(EnvelopePtr&& envelope, Channel to_channel) {
  // Lazily establish a new connection when necessary
  auto it = local_channel_to_socket_.find(to_channel);
  if (it == local_channel_to_socket_.end()) {
    if (auto br = broker_.lock()) {
      zmq::socket_t new_socket(*context_, ZMQ_PUSH);
      new_socket.connect("inproc://channel_" + std::to_string(to_channel));
      new_socket.set(zmq::sockopt::sndhwm, 0);
      auto res = local_channel_to_socket_.insert_or_assign(to_channel, move(new_socket));
      it = res.first;
    } else {
      // Broker has been destroyed. This can only happen during cleaning up
      return;
    }
  }

  SendEnvelope(it->second, move(envelope));
}

}  // namespace slog