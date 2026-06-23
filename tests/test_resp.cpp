#include <gtest/gtest.h>
#include "protocol/resp.h"

TEST(RespParser, Ping) {
  RESPReader reader;
  reader.feed("*1\r\n$4\r\nPING\r\n", 14);
  auto cmd = reader.try_parse();
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->name, "PING");
  EXPECT_TRUE(cmd->args.empty());
}

TEST(RespParser, Set) {
  RESPReader reader;
  std::string data = "*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n";
  reader.feed(data.data(), data.size());
  auto cmd = reader.try_parse();
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->name, "SET");
  ASSERT_EQ(cmd->args.size(), 2);
  EXPECT_EQ(cmd->args[0], "key");
  EXPECT_EQ(cmd->args[1], "value");
}

TEST(RespParser, Get) {
  RESPReader reader;
  reader.feed("*2\r\n$3\r\nGET\r\n$4\r\nname\r\n", 23);
  auto cmd = reader.try_parse();
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->name, "GET");
  ASSERT_EQ(cmd->args.size(), 1);
  EXPECT_EQ(cmd->args[0], "name");
}

TEST(RespParser, Del) {
  RESPReader reader;
  reader.feed("*2\r\n$3\r\nDEL\r\n$3\r\nkey\r\n", 22);
  auto cmd = reader.try_parse();
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->name, "DEL");
  ASSERT_EQ(cmd->args.size(), 1);
  EXPECT_EQ(cmd->args[0], "key");
}

TEST(RespParser, Exists) {
  RESPReader reader;
  std::string data = "*2\r\n$6\r\nEXISTS\r\n$3\r\nkey\r\n";
  reader.feed(data.data(), data.size());
  auto cmd = reader.try_parse();
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->name, "EXISTS");
  ASSERT_EQ(cmd->args.size(), 1);
}

TEST(RespParser, PartialDataReturnsNullopt) {
  RESPReader reader;
  reader.feed("*3\r\n$3\r\nSET\r\n$3\r\nke", 17);
  EXPECT_FALSE(reader.try_parse().has_value());
}

TEST(RespParser, EmptyFeed) {
  RESPReader reader;
  EXPECT_FALSE(reader.try_parse().has_value());
}

TEST(RespParser, PipelinedCommands) {
  RESPReader reader;
  std::string data = "*1\r\n$4\r\nPING\r\n*1\r\n$4\r\nPING\r\n";
  reader.feed(data.data(), data.size());
  auto cmd1 = reader.try_parse();
  ASSERT_TRUE(cmd1.has_value());
  EXPECT_EQ(cmd1->name, "PING");
  auto cmd2 = reader.try_parse();
  ASSERT_TRUE(cmd2.has_value());
  EXPECT_EQ(cmd2->name, "PING");
  EXPECT_FALSE(reader.try_parse().has_value());
}

TEST(RespParser, NullBulkString) {
  RESPReader reader;
  reader.feed("*2\r\n$3\r\nGET\r\n$-1\r\n", 19);
  auto cmd = reader.try_parse();
  ASSERT_TRUE(cmd.has_value());
  EXPECT_EQ(cmd->name, "GET");
  ASSERT_EQ(cmd->args.size(), 1);
  EXPECT_TRUE(cmd->args[0].empty());
}

TEST(RespSerialize, SimpleString) {
  Reply r = reply::Simple{"OK"};
  EXPECT_EQ(RESPReader::serialize(r), "+OK\r\n");
}

TEST(RespSerialize, BulkString) {
  Reply r = reply::Bulk{"hello"};
  EXPECT_EQ(RESPReader::serialize(r), "$5\r\nhello\r\n");
}

TEST(RespSerialize, NullBulk) {
  Reply r = reply::Nil{};
  EXPECT_EQ(RESPReader::serialize(r), "$-1\r\n");
}

TEST(RespSerialize, Integer) {
  Reply r = reply::Integer{42};
  EXPECT_EQ(RESPReader::serialize(r), ":42\r\n");
}

TEST(RespSerialize, Error) {
  Reply r = reply::Error{"something went wrong"};
  EXPECT_EQ(RESPReader::serialize(r), "-ERR something went wrong\r\n");
}

TEST(RespSerialize, EmptyBulkString) {
  Reply r = reply::Bulk{""};
  EXPECT_EQ(RESPReader::serialize(r), "$0\r\n\r\n");
}

TEST(RespSerialize, LargeInteger) {
  Reply r = reply::Integer{-1};
  EXPECT_EQ(RESPReader::serialize(r), ":-1\r\n");
}
