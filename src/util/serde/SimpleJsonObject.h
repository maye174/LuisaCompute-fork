#pragma once
#include <util/serde/Common.h>
#include <util/serde/SimpleJsonLoader.h>
#include <util/VGuid.h>
namespace toolhub::db {

class SimpleJsonLoader;
class SimpleJsonObject : public vstd::IDisposable{
protected:
	vstd::Guid selfGuid;
	SimpleBinaryJson* db = nullptr;
	SimpleJsonObject(
		vstd::Guid const& guid,
		SimpleBinaryJson* db);
	~SimpleJsonObject() {}

public:
	HashMap<vstd::Guid, std::pair<SimpleJsonObject*, uint8_t>>::Index dbIndexer;
	SimpleBinaryJson* GetDB() const { return db; }
	vstd::Guid const& GetGUID() const { return selfGuid; }
	virtual void M_GetSerData(std::vector<uint8_t>& data) = 0;
	virtual void LoadFromData(std::span<uint8_t const> data) = 0;
	virtual void AfterAdd(IDatabaseEvtVisitor* visitor) = 0;
	virtual void BeforeRemove(IDatabaseEvtVisitor* visitor) = 0;
};
}// namespace toolhub::db