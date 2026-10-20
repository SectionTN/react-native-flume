#include "bindings/Store.h"

namespace flume {

void Store::setRenderCell(std::shared_ptr<worklets::Serializable> renderCell) {
  std::lock_guard<std::mutex> lock(mutex_);
  renderCell_ = std::move(renderCell);
  renderCellVersion_++;
}

std::shared_ptr<worklets::Serializable> Store::renderCell() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return renderCell_;
}

uint64_t Store::renderCellVersion() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return renderCellVersion_;
}

}  // namespace flume
