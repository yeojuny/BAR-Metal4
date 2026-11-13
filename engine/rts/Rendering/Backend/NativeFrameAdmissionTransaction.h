/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */
#pragma once
#include <cstdint>
#include <utility>
namespace rendering {
// Main-owner transaction. No callbacks, allocation or throwing operation occurs
// between transfers. Caller reserves other resources before TransferValidated,
// and keeps capture blocked until settlement. Not a concurrent admission queue.
struct NativeNoWaterAdmission {
 void ValidateSubmittedCapacity(uint64_t) const {}
 void TransferPreparedFrame(uint64_t) noexcept {}
 void ValidateRestoreHeldFrame(uint64_t) const {}
 void RestoreHeldFrame(uint64_t) noexcept {}
 void QuarantineHeldFrame(uint64_t) noexcept {}
};
template<class UI,class Post,class Cloud,class Water=NativeNoWaterAdmission> class NativeFrameAdmissionTransaction {
 UI& ui;Post& post;Cloud& cloud;Water* water=nullptr;
 const std::uint64_t draw,serial;
public:
 NativeFrameAdmissionTransaction(UI& ui,Post& post,Cloud& cloud,std::uint64_t draw,std::uint64_t serial):ui(ui),post(post),cloud(cloud),draw(draw),serial(serial) {}
 NativeFrameAdmissionTransaction(UI& ui,Post& post,Cloud& cloud,Water& water,std::uint64_t draw,std::uint64_t serial):ui(ui),post(post),cloud(cloud),water(&water),draw(draw),serial(serial) {}
 void Validate() const {ui.ValidateSubmittedCapacity(draw);post.ValidateSubmittedCapacity(serial);cloud.ValidateSubmittedCapacity(draw);if(water)water->ValidateSubmittedCapacity(draw);}
 void TransferValidated() noexcept {
  static_assert(noexcept(ui.TransferPreparedFrame(draw)) && noexcept(post.TransferPreparedFrame(serial)) && noexcept(cloud.TransferPreparedFrame(draw)) && noexcept(water->TransferPreparedFrame(draw)));
  ui.TransferPreparedFrame(draw);post.TransferPreparedFrame(serial);cloud.TransferPreparedFrame(draw);if(water)water->TransferPreparedFrame(draw);
 }
 void Restore() {
  ui.ValidateRestoreHeldFrame(draw);post.ValidateRestoreHeldFrame(serial);cloud.ValidateRestoreHeldFrame(draw);if(water)water->ValidateRestoreHeldFrame(draw);
  static_assert(noexcept(ui.RestoreHeldFrame(draw)) && noexcept(post.RestoreHeldFrame(serial)) && noexcept(cloud.RestoreHeldFrame(draw)) && noexcept(water->RestoreHeldFrame(draw)));
  ui.RestoreHeldFrame(draw);post.RestoreHeldFrame(serial);cloud.RestoreHeldFrame(draw);if(water)water->RestoreHeldFrame(draw);
 }
 void Quarantine() noexcept {
  static_assert(noexcept(ui.QuarantineHeldFrame(draw)) && noexcept(post.QuarantineHeldFrame(serial)) && noexcept(cloud.QuarantineHeldFrame(draw)) && noexcept(water->QuarantineHeldFrame(draw)));
  ui.QuarantineHeldFrame(draw);post.QuarantineHeldFrame(serial);cloud.QuarantineHeldFrame(draw);if(water)water->QuarantineHeldFrame(draw);
 }
};
}
