#include "doctest.h"

#include "adapter/ListRows.h"

using namespace flume;

TEST_CASE("positions map to data indices around the header and footer") {
  ListRows rows{.header = true, .dataCount = 3, .footer = true};
  CHECK(rows.itemCount() == 5);
  std::vector<RowKind> kinds;
  std::vector<int> indices;
  for (int position = 0; position < 5; position++) {
    kinds.push_back(rows.kind(position));
    indices.push_back(rows.dataIndex(position));
  }
  CHECK(kinds == std::vector<RowKind>{RowKind::Header, RowKind::Data, RowKind::Data, RowKind::Data, RowKind::Footer});
  CHECK(indices == std::vector<int>{-1, 0, 1, 2, -1});
  CHECK(rows.position(0) == 1);
}

TEST_CASE("the empty row shows only with no data") {
  CHECK(ListRows{.header = true, .dataCount = 0, .emptyComponent = true, .footer = true}.kind(1) == RowKind::Empty);
  CHECK_FALSE(ListRows{.dataCount = 2, .emptyComponent = true}.hasEmptyRow());
  CHECK(ListRows{.dataCount = 0}.itemCount() == 0);
}

TEST_CASE("store ops at data indices land past the header") {
  ListOps ops;
  ops.removed = {{2, 1}};
  ops.inserted = {{0, 2}};
  ops.moved = {{1, 3}};
  ops.changed = {4};
  const ListOps positions = ListRows{.header = true, .dataCount = 6}.positions(ops);
  CHECK(positions.removed == std::vector<ListOps::Range>{{3, 1}});
  CHECK(positions.inserted == std::vector<ListOps::Range>{{1, 2}});
  CHECK(positions.moved == std::vector<std::pair<int, int>>{{2, 4}});
  CHECK(positions.changed == std::vector<int>{5});
  CHECK(ListRows{.dataCount = 6}.positions(ops).inserted == ops.inserted);
}

TEST_CASE("full span takes the header, sticky rows, the empty row and the footer") {
  ListRows rows{.header = true, .dataCount = 5, .footer = true};
  CHECK(rows.fullSpanPositions({2, 4, 9}) == std::vector<int>{0, 3, 5, 6});
}
