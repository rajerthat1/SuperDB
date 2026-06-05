#pragma once
#include "protocol/resp.h"
#include "storage/storage.h"

Reply handle_command(const Command& cmd, IStorageEngine& storage);
