#pragma once
#include <serialize/Common.h>
// Entry:
// toolhub::db::Database const* Database_GetFactory()
namespace toolhub::db {
class IJsonDatabase;
class Database {
public:
	[[nodiscard]] virtual IJsonDatabase* CreateDatabase() const = 0;
	[[nodiscard]] virtual IJsonDatabase* CreateConcurrentDatabase() const = 0;
};
}// namespace toolhub::db