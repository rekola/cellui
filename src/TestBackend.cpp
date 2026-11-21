#include <cellui/TestBackend.h>

#include <cellui/Utf8.h>

#include <cstdio>

namespace cellui::test {

TestBackend::TestBackend(int rows, int cols, DisplayInfo display)
  : display_(std::move(display)), rows_(rows), cols_(cols), frame_(rows, cols) {
  frames_hook_ = loop_.onFrame([this] { gate_.run(FrameGate::Clock::now()); });
}

TestBackend::~TestBackend() = default;

std::vector<BackendEvent> TestBackend::poll() {
  input_.drain();
  wake_.drain();
  std::vector<BackendEvent> out(queue_.begin(), queue_.end());
  queue_.clear();
  return out;
}

void TestBackend::push(const BackendEvent & event) {
  queue_.push_back(event);
  input_.signal();
}

void TestBackend::pushText(const std::string & text) {
  for (const std::string & cluster : utf8::clusters(text)) {
    // The first codepoint of the cluster, decoded.
    unsigned char lead = static_cast<unsigned char>(cluster[0]);
    int len = lead < 0x80 ? 1 : lead < 0xE0 ? 2 : lead < 0xF0 ? 3 : 4;
    int cp = len == 1 ? lead : lead & (0xFF >> (len + 1));
    for (int i = 1; i < len && static_cast<size_t>(i) < cluster.size(); i++) cp = (cp << 6) | (static_cast<unsigned char>(cluster[static_cast<size_t>(i)]) & 0x3F);
    pushKey(cp);
  }
}

void TestBackend::pushMouse(int y, int x, int key, unsigned mods, Input::Kind kind) {
  push(BackendEvent::makeInput(Input(key, y, x, mods & kModAlt, mods & kModShift, mods & kModCtrl, mods & (kModMeta | kModSuper), kind)));
}

void TestBackend::pushMotion(int y, int x, unsigned mods) {
  pushMouse(y, x, Key::Motion, mods, Input::Kind::RELEASE);
}

void TestBackend::pushPaste(const std::string & text) {
  push(BackendEvent::makePaste(text));
}

void TestBackend::resize(int rows, int cols) {
  rows_ = rows;
  cols_ = cols;
  push(BackendEvent::makeResize(rows, cols));
}

void TestBackend::pushWindowFocus(bool focused) {
  push(BackendEvent::makeFocus(focused));
}

void TestBackend::pushEnd() {
  push(BackendEvent::makeClose());
}

void TestBackend::wake() {
  wake_.signal();
}

void TestBackend::present(const CellBuffer & frame, const CursorRequest & cursor, const std::vector<ImagePlacement> & images,
                           const FrameInfo * info) {
  info_ = info ? *info : FrameInfo();
  cursor_ = cursor;
  images_ = images;
  frame_ = frame;
  if (!info_.backdrops.empty() || backdrops_.holdsState()) {
    if (emulate_backdrops_) {
      backdrops_.apply(frame_, images_, info_.backdrops, display_);
    } else if (!info_.backdrops.empty()) {
      // Each layer under the next, unblurred.
      auto over = [](CellBuffer & out, const CellBuffer & cells) {
        for (int r = 0; r < std::min(out.rows(), cells.rows()); r++) {
          for (int c = 0; c < std::min(out.cols(), cells.cols()); c++) {
            if (!CellBuffer::isBlank(cells.at(r, c))) out.put(r, c, cells.at(r, c));
          }
        }
      };
      // A plane scrolled by part of a cell is shown where its cells are; the
      // fraction is in the layer for the test to read.
      auto overScroll = [&](CellBuffer & out, const BackdropLayer & layer) {
        if (!layer.scroll) return;
        const ScrollContent & content = *layer.scroll;
        for (int r = 0; r < content.rows; r++) {
          for (int c = 0; c < content.cols; c++) {
            if (!CellBuffer::isBlank(content.cells.at(r, c))) out.put(content.row + r, content.col + c, content.cells.at(r, c));
          }
        }
      };
      CellBuffer stacked = info_.backdrops[0].cells;
      overScroll(stacked, info_.backdrops[0]);
      for (size_t i = 1; i < info_.backdrops.size(); i++) {
        over(stacked, info_.backdrops[i].cells);
        overScroll(stacked, info_.backdrops[i]);
      }
      over(stacked, frame_);
      frame_ = std::move(stacked);
    }
  }
  frames_++;
}


void TestBackend::setHdr(std::optional<HdrInfo> hdr) {
  display_.hdr = hdr;
  push(BackendEvent::makeDisplay());
}

CellBuffer TestBackend::sdrFrame() const {
  CellBuffer out = frame_;
  auto clip = [](std::optional<Color> & c) {
    if (c) {
      auto rgb = c->toSrgb8();
      c = Color(rgb[0], rgb[1], rgb[2]);
    }
  };
  for (int r = 0; r < out.rows(); r++) {
    for (int c = 0; c < out.cols(); c++) {
      CellBuffer::Cell cell = out.at(r, c);
      clip(cell.fg);
      clip(cell.bg);
      out.put(r, c, cell);
    }
  }
  return out;
}

}  // namespace cellui::test
