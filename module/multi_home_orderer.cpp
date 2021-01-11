#include "module/multi_home_orderer.h"

#include <glog/logging.h>

#include "common/constants.h"
#include "common/monitor.h"
#include "common/proto_utils.h"
#include "module/ticker.h"
#include "paxos/simple_multi_paxos.h"

namespace slog {

using internal::Batch;
using internal::Envelope;
using internal::Request;

MultiHomeOrderer::MultiHomeOrderer(const ConfigurationPtr& config, const shared_ptr<Broker>& broker,
                                   std::chrono::milliseconds poll_timeout)
    : NetworkedModule("MultiHomeOrderer", broker, kMultiHomeOrdererChannel, poll_timeout),
      config_(config),
      batch_id_counter_(0) {
  NewBatch();
}

std::vector<zmq::socket_t> MultiHomeOrderer::InitializeCustomSockets() {
  vector<zmq::socket_t> ticker_socket;
  ticker_socket.push_back(Ticker::Subscribe(*context()));
  return ticker_socket;
}

void MultiHomeOrderer::NewBatch() {
  if (batch_ == nullptr) {
    batch_.reset(new Batch());
  }
  batch_->Clear();
  batch_->set_transaction_type(TransactionType::MULTI_HOME);
}

void MultiHomeOrderer::HandleInternalRequest(EnvelopePtr&& env) {
  auto request = env->mutable_request();
  switch (request->type_case()) {
    case Request::kForwardTxn: {
      // Received a new multi-home txn
      auto txn = request->mutable_forward_txn()->release_txn();

      TRACE(txn->mutable_internal(), TransactionEvent::ENTER_MULTI_HOME_ORDERER);

      batch_->mutable_transactions()->AddAllocated(txn);
      break;
    }
    case Request::kForwardBatch:
      // Received a batch of multi-home txn replicated from another region
      ProcessForwardBatch(request->mutable_forward_batch());
      break;
    default:
      LOG(ERROR) << "Unexpected request type received: \"" << CASE_NAME(request->type_case(), Request) << "\"";
      break;
  }
}

void MultiHomeOrderer::HandleCustomSocket(zmq::socket_t& socket, size_t /* socket_index */) {
  // Remove the dummy message out of the queue
  if (zmq::message_t msg; !socket.recv(msg, zmq::recv_flags::dontwait)) {
    return;
  }

  if (batch_->transactions().empty()) {
    return;
  }

  auto batch_id = NextBatchId();
  batch_->set_id(batch_id);

  VLOG(1) << "Finished multi-home batch " << batch_id << ". Sending out for ordering and replicating";

  // Make a proposal for multi-home batch ordering
  auto paxos_env = NewEnvelope();
  auto paxos_propose = paxos_env->mutable_request()->mutable_paxos_propose();
  paxos_propose->set_value(batch_id);
  Send(move(paxos_env), kGlobalPaxos);

  // Replicate new batch to other regions
  Envelope batch_env;
  auto forward_batch = batch_env.mutable_request()->mutable_forward_batch();
  forward_batch->set_allocated_batch_data(batch_.release());
  auto part = config_->leader_partition_for_multi_home_ordering();
  auto num_replicas = config_->num_replicas();
  for (uint32_t rep = 0; rep < num_replicas; rep++) {
    auto machine_id = config_->MakeMachineId(rep, part);
    Send(batch_env, machine_id, kMultiHomeOrdererChannel);
  }
  forward_batch->release_batch_data();

  NewBatch();
}

void MultiHomeOrderer::ProcessForwardBatch(internal::ForwardBatch* forward_batch) {
  switch (forward_batch->part_case()) {
    case internal::ForwardBatch::kBatchData: {
      auto batch = BatchPtr(forward_batch->release_batch_data());

      TRACE(batch.get(), TransactionEvent::ENTER_MULTI_HOME_ORDERER_IN_BATCH);

      multi_home_batch_log_.AddBatch(std::move(batch));
      break;
    }
    case internal::ForwardBatch::kBatchOrder: {
      auto& batch_order = forward_batch->batch_order();
      multi_home_batch_log_.AddSlot(batch_order.slot(), batch_order.batch_id());
      break;
    }
    default:
      break;
  }

  while (multi_home_batch_log_.HasNextBatch()) {
    auto batch_and_slot = multi_home_batch_log_.NextBatch();
    auto slot = batch_and_slot.first;
    auto& batch = batch_and_slot.second;

    // Replace the batch id with its slot number so that it is
    // easier to determine the batch order later on
    batch->set_id(slot);

    auto env = NewEnvelope();
    auto forward_batch = env->mutable_request()->mutable_forward_batch();
    forward_batch->set_allocated_batch_data(batch.release());

    TRACE(forward_batch->mutable_batch_data(), TransactionEvent::EXIT_MULTI_HOME_ORDERER_IN_BATCH);

    // Send the newly ordered multi-home batch to the sequencer
    Send(move(env), kSequencerChannel);
  }
}

BatchId MultiHomeOrderer::NextBatchId() {
  batch_id_counter_++;
  return batch_id_counter_ * kMaxNumMachines + config_->local_machine_id();
}

}  // namespace slog