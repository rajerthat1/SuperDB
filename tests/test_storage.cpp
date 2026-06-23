#include <gtest/gtest.h>
#include "storage/in_memory_storage.h"
#include "storage/wal_engine.h"
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

TEST(InMemoryEngine, SetAndGet) {
  InMemoryEngine engine;
  engine.set("key", "value");
  auto val = engine.get("key");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, "value");
}

TEST(InMemoryEngine, GetNonexistent) {
  InMemoryEngine engine;
  EXPECT_FALSE(engine.get("nonexistent").has_value());
}

TEST(InMemoryEngine, Overwrite) {
  InMemoryEngine engine;
  engine.set("key", "value1");
  engine.set("key", "value2");
  auto val = engine.get("key");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, "value2");
}

TEST(InMemoryEngine, Del) {
  InMemoryEngine engine;
  engine.set("key", "value");
  EXPECT_TRUE(engine.del("key"));
  EXPECT_FALSE(engine.get("key").has_value());
}

TEST(InMemoryEngine, DelNonexistent) {
  InMemoryEngine engine;
  EXPECT_FALSE(engine.del("nonexistent"));
}

TEST(InMemoryEngine, Exists) {
  InMemoryEngine engine;
  EXPECT_FALSE(engine.exists("key"));
  engine.set("key", "value");
  EXPECT_TRUE(engine.exists("key"));
  engine.del("key");
  EXPECT_FALSE(engine.exists("key"));
}

TEST(InMemoryEngine, EmptyKey) {
  InMemoryEngine engine;
  engine.set("", "empty");
  auto val = engine.get("");
  ASSERT_TRUE(val.has_value());
  EXPECT_EQ(*val, "empty");
}

TEST(InMemoryEngine, EmptyValue) {
  InMemoryEngine engine;
  engine.set("key", "");
  auto val = engine.get("key");
  ASSERT_TRUE(val.has_value());
  EXPECT_TRUE(val->empty());
}

class WalTest : public ::testing::Test {
protected:
  const char* path_ = "/tmp/test_wal.log";

  void SetUp() override { std::remove(path_); }
  void TearDown() override { std::remove(path_); }
};

TEST_F(WalTest, RecoverAfterCrash) {
  {
    InMemoryEngine inner;
    WalEngine wal(inner, path_);
    wal.set("a", "1");
    wal.set("b", "2");
  }

  {
    InMemoryEngine inner;
    WalEngine wal(inner, path_);
    wal.replay();
    auto a = wal.get("a");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(*a, "1");
    auto b = wal.get("b");
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(*b, "2");
  }
}

TEST_F(WalTest, NoDataOnFirstRun) {
  InMemoryEngine inner;
  WalEngine wal(inner, path_);
  wal.replay();
  EXPECT_FALSE(wal.get("x").has_value());
}

TEST_F(WalTest, DeleteLogged) {
  {
    InMemoryEngine inner;
    WalEngine wal(inner, path_);
    wal.set("key", "val");
    wal.del("key");
  }

  {
    InMemoryEngine inner;
    WalEngine wal(inner, path_);
    wal.replay();
    EXPECT_FALSE(wal.get("key").has_value());
  }
}

TEST_F(WalTest, OverwriteInWAL) {
  {
    InMemoryEngine inner;
    WalEngine wal(inner, path_);
    wal.set("key", "old");
    wal.set("key", "new");
  }

  {
    InMemoryEngine inner;
    WalEngine wal(inner, path_);
    wal.replay();
    auto val = wal.get("key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "new");
  }
}

TEST_F(WalTest, PartialLastEntryIgnored) {
  {
    InMemoryEngine inner;
    WalEngine wal(inner, path_);
    wal.set("a", "1");
  }

  int fd = open(path_, O_WRONLY | O_APPEND);
  ASSERT_GE(fd, 0);
  write(fd, "*3\r\n$3\r\nSET\r\n$3\r\nb\r\n$4\r\n", 22);
  close(fd);

  {
    InMemoryEngine inner;
    WalEngine wal(inner, path_);
    wal.replay();
    EXPECT_TRUE(wal.get("a").has_value());
    EXPECT_FALSE(wal.get("b").has_value());
  }
}
