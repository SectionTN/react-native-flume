#include "bindings/Store.h"

namespace flume {

Store::~Store() {
  if (reconciler != nullptr) {
    for (auto& [id, holder] : holders) reconciler->release(*holder);
  }
  holders.clear();
  reconciler.reset();
  layout.reset();
  measurer.reset();
}

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

void Store::setSeparatorCell(std::shared_ptr<worklets::Serializable> separatorCell) {
  std::lock_guard<std::mutex> lock(mutex_);
  separatorCell_ = std::move(separatorCell);
  separatorCellVersion_++;
}

std::pair<std::shared_ptr<worklets::Serializable>, uint64_t> Store::separatorCell() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return {separatorCell_, separatorCellVersion_};
}

}  // namespace flume
