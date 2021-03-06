/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <map>
#include <utility>

#include "base/strings/stringprintf.h"
#include "bat/ledger/internal/common/time_util.h"
#include "bat/ledger/internal/database/database_contribution_queue_publishers.h"
#include "bat/ledger/internal/database/database_util.h"
#include "bat/ledger/internal/ledger_impl.h"

using std::placeholders::_1;

namespace braveledger_database {

namespace {

const char kTableName[] = "contribution_queue_publishers";
const char kParentTableName[] = "contribution_queue";

}  // namespace

DatabaseContributionQueuePublishers::DatabaseContributionQueuePublishers(
    bat_ledger::LedgerImpl* ledger) :
    DatabaseTable(ledger) {
}

DatabaseContributionQueuePublishers::
~DatabaseContributionQueuePublishers() = default;

bool DatabaseContributionQueuePublishers::CreateTableV9(
    ledger::DBTransaction* transaction) {
  DCHECK(transaction);

  const std::string query = base::StringPrintf(
      "CREATE TABLE %s ("
        "contribution_queue_id INTEGER NOT NULL,"
        "publisher_key TEXT NOT NULL,"
        "amount_percent DOUBLE NOT NULL,"
        "CONSTRAINT fk_%s_publisher_key "
        "    FOREIGN KEY (publisher_key) "
        "    REFERENCES publisher_info (publisher_id),"
        "CONSTRAINT fk_%s_id "
        "    FOREIGN KEY (contribution_queue_id) "
        "    REFERENCES %s (contribution_queue_id) "
        "    ON DELETE CASCADE"
      ")",
      kTableName,
      kTableName,
      kTableName,
      kParentTableName);

  auto command = ledger::DBCommand::New();
  command->type = ledger::DBCommand::Type::EXECUTE;
  command->command = query;
  transaction->commands.push_back(std::move(command));

  return true;
}

bool DatabaseContributionQueuePublishers::CreateTableV15(
    ledger::DBTransaction* transaction) {
  DCHECK(transaction);

  const std::string query = base::StringPrintf(
      "CREATE TABLE %s ("
        "contribution_queue_id INTEGER NOT NULL,"
        "publisher_key TEXT NOT NULL,"
        "amount_percent DOUBLE NOT NULL"
      ")",
      kTableName);

  auto command = ledger::DBCommand::New();
  command->type = ledger::DBCommand::Type::EXECUTE;
  command->command = query;
  transaction->commands.push_back(std::move(command));

  return true;
}

bool DatabaseContributionQueuePublishers::CreateTableV23(
    ledger::DBTransaction* transaction) {
  DCHECK(transaction);

  const std::string query = base::StringPrintf(
      "CREATE TABLE %s ("
        "contribution_queue_id TEXT NOT NULL,"
        "publisher_key TEXT NOT NULL,"
        "amount_percent DOUBLE NOT NULL"
      ")",
      kTableName);

  auto command = ledger::DBCommand::New();
  command->type = ledger::DBCommand::Type::EXECUTE;
  command->command = query;
  transaction->commands.push_back(std::move(command));

  return true;
}

bool DatabaseContributionQueuePublishers::CreateIndexV15(
    ledger::DBTransaction* transaction) {
  DCHECK(transaction);

  bool success =
      this->InsertIndex(transaction, kTableName, "contribution_queue_id");

  if (!success) {
    return false;
  }

  return this->InsertIndex(transaction, kTableName, "publisher_key");
}

bool DatabaseContributionQueuePublishers::CreateIndexV23(
    ledger::DBTransaction* transaction) {
  DCHECK(transaction);

  bool success =
      this->InsertIndex(transaction, kTableName, "contribution_queue_id");

  if (!success) {
    return false;
  }

  return this->InsertIndex(transaction, kTableName, "publisher_key");
}

bool DatabaseContributionQueuePublishers::Migrate(
    ledger::DBTransaction* transaction,
    const int target) {
  DCHECK(transaction);

  switch (target) {
    case 9: {
      return MigrateToV9(transaction);
    }
    case 15: {
      return MigrateToV15(transaction);
    }
    case 23: {
      return MigrateToV23(transaction);
    }
    default: {
      return true;
    }
  }
}

bool DatabaseContributionQueuePublishers::MigrateToV9(
    ledger::DBTransaction* transaction) {
  DCHECK(transaction);

  if (!DropTable(transaction, kTableName)) {
    BLOG(0, "Table couldn't be dropped");
    return false;
  }

  if (!CreateTableV9(transaction)) {
    BLOG(0, "Table couldn't be created");
    return false;
  }

  return true;
}

bool DatabaseContributionQueuePublishers::MigrateToV15(
    ledger::DBTransaction* transaction) {
  DCHECK(transaction);

  const std::string temp_table_name = base::StringPrintf(
      "%s_temp",
      kTableName);

  if (!RenameDBTable(transaction, kTableName, temp_table_name)) {
    BLOG(0, "Table couldn't be renamed");
    return false;
  }

  if (!CreateTableV15(transaction)) {
    BLOG(0, "Table couldn't be created");
    return false;
  }

  if (!CreateIndexV15(transaction)) {
    BLOG(0, "Index couldn't be created");
    return false;
  }

  const std::map<std::string, std::string> columns = {
    { "contribution_queue_id", "contribution_queue_id" },
    { "publisher_key", "publisher_key" },
    { "amount_percent", "amount_percent" }
  };

  if (!MigrateDBTable(
      transaction,
      temp_table_name,
      kTableName,
      columns,
      true)) {
    BLOG(0, "Table migration failed");
    return false;
  }
  return true;
}

bool DatabaseContributionQueuePublishers::MigrateToV23(
    ledger::DBTransaction* transaction) {
  DCHECK(transaction);

  const std::string temp_table_name = base::StringPrintf(
      "%s_temp",
      kTableName);

  if (!RenameDBTable(transaction, kTableName, temp_table_name)) {
    BLOG(0, "Table couldn't be renamed");
    return false;
  }

  std::string query =
      "DROP INDEX IF EXISTS "
      "contribution_queue_publishers_contribution_queue_id_index;"
      " DROP INDEX IF EXISTS "
      "contribution_queue_publishers_publisher_key_index;";
  auto command = ledger::DBCommand::New();
  command->type = ledger::DBCommand::Type::EXECUTE;
  command->command = query;
  transaction->commands.push_back(std::move(command));

  if (!CreateTableV23(transaction)) {
    BLOG(0, "Table couldn't be created");
    return false;
  }

  if (!CreateIndexV23(transaction)) {
    BLOG(0, "Index couldn't be created");
    return false;
  }

  // Migrate the contribution_queue table
  query = base::StringPrintf(
      "INSERT INTO %s "
      "(contribution_queue_id, publisher_key, amount_percent) "
      "SELECT CAST(contribution_queue_id AS TEXT), publisher_key, "
      "amount_percent FROM %s",
      kTableName,
      temp_table_name.c_str());

  command = ledger::DBCommand::New();
  command->type = ledger::DBCommand::Type::EXECUTE;
  command->command = query;
  transaction->commands.push_back(std::move(command));

  if (!DropTable(transaction, temp_table_name)) {
    BLOG(0, "Table couldn't be dropped");
    return false;
  }

  return true;
}

void DatabaseContributionQueuePublishers::InsertOrUpdate(
    const std::string& id,
    ledger::ContributionQueuePublisherList list,
    ledger::ResultCallback callback) {
  if (id.empty() || list.empty()) {
    BLOG(1, "Empty data");
    callback(ledger::Result::LEDGER_ERROR);
    return;
  }

  auto transaction = ledger::DBTransaction::New();

  const std::string query = base::StringPrintf(
      "INSERT OR REPLACE INTO %s "
      "(contribution_queue_id, publisher_key, amount_percent) VALUES (?, ?, ?)",
      kTableName);

  auto command = ledger::DBCommand::New();
  command->type = ledger::DBCommand::Type::RUN;
  command->command = query;

  for (const auto& publisher : list) {
    BindString(command.get(), 0, id);
    BindString(command.get(), 1, publisher->publisher_key);
    BindDouble(command.get(), 2, publisher->amount_percent);

    transaction->commands.push_back(command->Clone());
  }

  auto transaction_callback = std::bind(&OnResultCallback,
      _1,
      callback);

  ledger_->RunDBTransaction(std::move(transaction), transaction_callback);
}

void DatabaseContributionQueuePublishers::GetRecordsByQueueId(
    const std::string& queue_id,
    ContributionQueuePublishersListCallback callback) {
  if (queue_id.empty()) {
    BLOG(1, "Queue id is empty");
    callback({});
    return;
  }

  auto transaction = ledger::DBTransaction::New();

  const std::string query = base::StringPrintf(
      "SELECT publisher_key, amount_percent "
      "FROM %s WHERE contribution_queue_id = ?",
      kTableName);

  auto command = ledger::DBCommand::New();
  command->type = ledger::DBCommand::Type::READ;
  command->command = query;

  BindString(command.get(), 0, queue_id);

  command->record_bindings = {
      ledger::DBCommand::RecordBindingType::STRING_TYPE,
      ledger::DBCommand::RecordBindingType::DOUBLE_TYPE
  };

  transaction->commands.push_back(std::move(command));

  auto transaction_callback =
      std::bind(&DatabaseContributionQueuePublishers::OnGetRecordsByQueueId,
          this,
          _1,
          callback);

  ledger_->RunDBTransaction(std::move(transaction), transaction_callback);
}

void DatabaseContributionQueuePublishers::OnGetRecordsByQueueId(
    ledger::DBCommandResponsePtr response,
    ContributionQueuePublishersListCallback callback) {
  if (!response ||
      response->status != ledger::DBCommandResponse::Status::RESPONSE_OK) {
    BLOG(0, "Response is wrong");
    callback({});
    return;
  }

  ledger::ContributionQueuePublisherList list;
  for (auto const& record : response->result->get_records()) {
    auto info = ledger::ContributionQueuePublisher::New();
    auto* record_pointer = record.get();

    info->publisher_key = GetStringColumn(record_pointer, 0);
    info->amount_percent = GetDoubleColumn(record_pointer, 1);

    list.push_back(std::move(info));
  }

  callback(std::move(list));
}

}  // namespace braveledger_database
