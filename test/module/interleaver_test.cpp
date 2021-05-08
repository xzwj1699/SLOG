#include "module/interleaver.h"

#include <gtest/gtest.h>

#include <vector>

#include "common/proto_utils.h"
#include "test/test_utils.h"

using namespace std;
using namespace slog;

using internal::Envelope;

TEST(LocalLogTest, InOrder) {
  LocalLog interleaver;
  interleaver.AddBatchId(111 /* queue_id */, 0 /* position */, 100 /* batch_id */);
  ASSERT_FALSE(interleaver.HasNextBatch());

  interleaver.AddSlot(0 /* slot_id */, 111 /* queue_id */, 0 /* leader */);
  ASSERT_EQ(make_pair(0U, make_pair(100UL, 0)), interleaver.NextBatch());

  interleaver.AddBatchId(222 /* queue_id */, 0 /* position */, 200 /* batch_id */);
  ASSERT_FALSE(interleaver.HasNextBatch());

  interleaver.AddSlot(1 /* slot_id */, 222 /* queue_id */, 1 /* leader */);
  ASSERT_EQ(make_pair(1U, make_pair(200UL, 1)), interleaver.NextBatch());

  ASSERT_FALSE(interleaver.HasNextBatch());
}

TEST(LocalLogTest, BatchesComeFirst) {
  LocalLog interleaver;

  interleaver.AddBatchId(222 /* queue_id */, 0 /* position */, 100 /* batch_id */);
  interleaver.AddBatchId(111 /* queue_id */, 0 /* position */, 200 /* batch_id */);
  interleaver.AddBatchId(333 /* queue_id */, 0 /* position */, 300 /* batch_id */);
  interleaver.AddBatchId(333 /* queue_id */, 1 /* position */, 400 /* batch_id */);

  interleaver.AddSlot(0 /* slot_id */, 111 /* queue_id */, 0 /* leader */);
  ASSERT_EQ(make_pair(0U, make_pair(200UL, 0)), interleaver.NextBatch());

  interleaver.AddSlot(1 /* slot_id */, 333 /* queue_id */, 1 /* leader */);
  ASSERT_EQ(make_pair(1U, make_pair(300UL, 1)), interleaver.NextBatch());

  interleaver.AddSlot(2 /* slot_id */, 222 /* queue_id */, 2 /* leader */);
  ASSERT_EQ(make_pair(2U, make_pair(100UL, 2)), interleaver.NextBatch());

  interleaver.AddSlot(3 /* slot_id */, 333 /* queue_id */, 3 /* leader */);
  ASSERT_EQ(make_pair(3U, make_pair(400UL, 3)), interleaver.NextBatch());

  ASSERT_FALSE(interleaver.HasNextBatch());
}

TEST(LocalLogTest, SlotsComeFirst) {
  LocalLog interleaver;

  interleaver.AddSlot(2 /* slot_id */, 222 /* queue_id */, 0 /* leader */);
  interleaver.AddSlot(1 /* slot_id */, 333 /* queue_id */, 0 /* leader */);
  interleaver.AddSlot(3 /* slot_id */, 333 /* queue_id */, 0 /* leader */);
  interleaver.AddSlot(0 /* slot_id */, 111 /* queue_id */, 0 /* leader */);

  interleaver.AddBatchId(111 /* queue_id */, 0 /* position */, 200 /* batch_id */);
  ASSERT_EQ(make_pair(0U, make_pair(200UL, 0)), interleaver.NextBatch());

  interleaver.AddBatchId(333 /* queue_id */, 0 /* position */, 300 /* batch_id */);
  ASSERT_EQ(make_pair(1U, make_pair(300UL, 0)), interleaver.NextBatch());

  interleaver.AddBatchId(222 /* queue_id */, 0 /* position */, 100 /* batch_id */);
  ASSERT_EQ(make_pair(2U, make_pair(100UL, 0)), interleaver.NextBatch());

  interleaver.AddBatchId(333 /* queue_id */, 1 /* position */, 400 /* batch_id */);
  ASSERT_EQ(make_pair(3U, make_pair(400UL, 0)), interleaver.NextBatch());

  ASSERT_FALSE(interleaver.HasNextBatch());
}

TEST(LocalLogTest, MultipleNextBatches) {
  LocalLog interleaver;

  interleaver.AddBatchId(111 /* queue_id */, 0 /* position */, 300 /* batch_id */);
  interleaver.AddBatchId(222 /* queue_id */, 0 /* position */, 100 /* batch_id */);
  interleaver.AddBatchId(333 /* queue_id */, 0 /* position */, 400 /* batch_id */);
  interleaver.AddBatchId(333 /* queue_id */, 1 /* position */, 200 /* batch_id */);

  interleaver.AddSlot(3 /* slot_id */, 333 /* queue_id */, 1 /* leader */);
  interleaver.AddSlot(1 /* slot_id */, 333 /* queue_id */, 1 /* leader */);
  interleaver.AddSlot(2 /* slot_id */, 111 /* queue_id */, 1 /* leader */);
  interleaver.AddSlot(0 /* slot_id */, 222 /* queue_id */, 1 /* leader */);

  ASSERT_EQ(make_pair(0U, make_pair(100UL, 1)), interleaver.NextBatch());
  ASSERT_EQ(make_pair(1U, make_pair(400UL, 1)), interleaver.NextBatch());
  ASSERT_EQ(make_pair(2U, make_pair(300UL, 1)), interleaver.NextBatch());
  ASSERT_EQ(make_pair(3U, make_pair(200UL, 1)), interleaver.NextBatch());

  ASSERT_FALSE(interleaver.HasNextBatch());
}

TEST(LocalLogTest, SameOriginOutOfOrder) {
  LocalLog interleaver;

  interleaver.AddBatchId(111 /* queue_id */, 1 /* position */, 200 /* batch_id */);
  interleaver.AddBatchId(111 /* queue_id */, 2 /* position */, 300 /* batch_id */);

  interleaver.AddSlot(0 /* slot_id */, 111 /* queue_id */, 0 /* leader */);
  ASSERT_FALSE(interleaver.HasNextBatch());

  interleaver.AddSlot(1 /* slot_id */, 111 /* queue_id */, 0 /* leader */);
  ASSERT_FALSE(interleaver.HasNextBatch());

  interleaver.AddBatchId(111 /* queue_id */, 0 /* position */, 100 /* batch_id */);

  interleaver.AddSlot(2 /* slot_id */, 111 /* queue_id */, 0 /* leader */);
  ASSERT_TRUE(interleaver.HasNextBatch());

  ASSERT_EQ(make_pair(0U, make_pair(100UL, 0)), interleaver.NextBatch());
  ASSERT_EQ(make_pair(1U, make_pair(200UL, 0)), interleaver.NextBatch());
  ASSERT_EQ(make_pair(2U, make_pair(300UL, 0)), interleaver.NextBatch());

  ASSERT_FALSE(interleaver.HasNextBatch());
}

const int NUM_REPLICAS = 2;
const int NUM_PARTITIONS = 2;
constexpr int NUM_MACHINES = NUM_REPLICAS * NUM_PARTITIONS;

class InterleaverTest : public ::testing::Test {
 public:
  void SetUp() {
    auto configs = MakeTestConfigurations("interleaver", NUM_REPLICAS, NUM_PARTITIONS);
    for (int i = 0; i < 4; i++) {
      slogs_[i] = make_unique<TestSlog>(configs[i]);
      slogs_[i]->AddInterleaver();
      slogs_[i]->AddOutputChannel(kSchedulerChannel);
      senders_[i] = slogs_[i]->NewSender();
      slogs_[i]->StartInNewThreads();
    }
  }

  void SendToInterleaver(int from, int to, const Envelope& req) {
    senders_[from]->Send(req, to, kInterleaverChannel);
  }
  void SendToLocalQueue(int from, int to, const Envelope& req) {
    auto copied = std::make_unique<Envelope>(req);
    senders_[from]->Send(std::move(copied), to, kLocalLogChannel);
  }

  Transaction* ReceiveTxn(int i) {
    auto req_env = slogs_[i]->ReceiveFromOutputChannel(kSchedulerChannel);
    if (req_env == nullptr) {
      return nullptr;
    }
    if (req_env->request().type_case() != internal::Request::kForwardTxn) {
      return nullptr;
    }
    return req_env->mutable_request()->mutable_forward_txn()->release_txn();
  }

  unique_ptr<Sender> senders_[4];
  unique_ptr<TestSlog> slogs_[4];
};

internal::Batch* MakeBatch(BatchId batch_id, const vector<Transaction*>& txns, TransactionType batch_type) {
  internal::Batch* batch = new internal::Batch();
  batch->set_id(batch_id);
  batch->set_transaction_type(batch_type);
  for (auto txn : txns) {
    batch->mutable_transactions()->AddAllocated(txn);
  }
  return batch;
}

TEST_F(InterleaverTest, BatchDataBeforeBatchOrder) {
  auto expected_txn_1 = MakeTransaction({{"A"}, {"B", KeyType::WRITE}});
  auto expected_txn_2 = MakeTransaction({{"X"}, {"Y", KeyType::WRITE}});
  auto batch = MakeBatch(100, {expected_txn_1, expected_txn_2}, SINGLE_HOME);

  // Replicate batch data to all machines
  {
    Envelope req;
    auto forward_batch = req.mutable_request()->mutable_forward_batch();
    forward_batch->mutable_batch_data()->CopyFrom(*batch);
    forward_batch->set_same_origin_position(0);

    for (int i = 0; i < NUM_MACHINES; i++) {
      SendToInterleaver(0, i, req);
    }
  }

  // Then send local ordering
  {
    Envelope req;
    auto local_batch_order = req.mutable_request()->mutable_forward_batch()->mutable_local_batch_order();
    local_batch_order->set_queue_id(0);
    local_batch_order->set_slot(0);
    local_batch_order->set_leader(0);
    SendToLocalQueue(0, 0, req);
    SendToLocalQueue(1, 1, req);

    // The batch order is replicated across all machines
    for (int i = 0; i < NUM_MACHINES; i++) {
      auto txn1 = ReceiveTxn(i);
      auto txn2 = ReceiveTxn(i);
      ASSERT_EQ(*txn1, *expected_txn_1);
      ASSERT_EQ(*txn2, *expected_txn_2);
      delete txn1;
      delete txn2;
    }
  }

  delete batch;
}

TEST_F(InterleaverTest, BatchOrderBeforeBatchData) {
  auto expected_txn_1 = MakeTransaction({{"A"}, {"B", KeyType::WRITE}});
  auto expected_txn_2 = MakeTransaction({{"X"}, {"Y", KeyType::WRITE}});
  auto batch = MakeBatch(100, {expected_txn_1, expected_txn_2}, SINGLE_HOME);

  // Then send local ordering
  {
    Envelope req;
    auto local_batch_order = req.mutable_request()->mutable_forward_batch()->mutable_local_batch_order();
    local_batch_order->set_queue_id(0);
    local_batch_order->set_slot(0);
    local_batch_order->set_leader(0);
    SendToLocalQueue(0, 0, req);
    SendToLocalQueue(1, 1, req);
  }

  // Replicate batch data to all machines
  {
    Envelope req;
    auto forward_batch = req.mutable_request()->mutable_forward_batch();
    forward_batch->mutable_batch_data()->CopyFrom(*batch);
    forward_batch->set_same_origin_position(0);

    for (int i = 0; i < NUM_MACHINES; i++) {
      SendToInterleaver(0, i, req);
    }

    // Both batch data and batch order are sent at the same time
    for (int i = 0; i < NUM_MACHINES; i++) {
      auto txn1 = ReceiveTxn(i);
      auto txn2 = ReceiveTxn(i);
      ASSERT_EQ(*txn1, *expected_txn_1);
      ASSERT_EQ(*txn2, *expected_txn_2);
      delete txn1;
      delete txn2;
    }
  }

  delete batch;
}

TEST_F(InterleaverTest, TwoBatches) {
  auto sh_txn_1 = MakeTransaction({{"A"}, {"B", KeyType::WRITE}});
  auto sh_batch_1 = MakeBatch(100, {sh_txn_1}, SINGLE_HOME);

  auto sh_txn_2 = MakeTransaction({{"M"}, {"N", KeyType::WRITE}});
  auto sh_batch_2 = MakeBatch(200, {sh_txn_2}, SINGLE_HOME);

  // Replicate batch data to all machines
  {
    Envelope req1;
    auto forward_batch1 = req1.mutable_request()->mutable_forward_batch();
    forward_batch1->mutable_batch_data()->CopyFrom(*sh_batch_1);
    forward_batch1->set_same_origin_position(0);

    Envelope req2;
    auto forward_batch2 = req2.mutable_request()->mutable_forward_batch();
    forward_batch2->mutable_batch_data()->CopyFrom(*sh_batch_2);
    forward_batch2->set_same_origin_position(0);

    for (int i = 0; i < NUM_MACHINES; i++) {
      SendToInterleaver(0, i, req1);
      SendToInterleaver(1, i, req2);
    }
  }

  // Then send local ordering. Txn 1 is ordered after txn 2
  {
    Envelope req1;
    auto local_batch_order1 = req1.mutable_request()->mutable_forward_batch()->mutable_local_batch_order();
    local_batch_order1->set_slot(0);
    local_batch_order1->set_queue_id(1);
    local_batch_order1->set_leader(0);
    SendToLocalQueue(0, 0, req1);
    SendToLocalQueue(1, 1, req1);

    for (int i = 0; i < NUM_MACHINES; i++) {
      auto txn = ReceiveTxn(i);
      ASSERT_EQ(*txn, *sh_txn_2);
      delete txn;
    }

    Envelope req2;
    auto local_batch_order2 = req2.mutable_request()->mutable_forward_batch()->mutable_local_batch_order();
    local_batch_order2->set_slot(1);
    local_batch_order2->set_queue_id(0);
    local_batch_order2->set_leader(1);
    SendToLocalQueue(0, 0, req2);
    SendToLocalQueue(1, 1, req2);

    for (int i = 0; i < NUM_MACHINES; i++) {
      auto txn = ReceiveTxn(i);
      ASSERT_EQ(*txn, *sh_txn_1);
      delete txn;
    }
  }
}
