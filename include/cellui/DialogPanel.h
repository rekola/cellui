#pragma once

#include <cellui/Plane.h>

#include <memory>

// What every dialog's box looks like, so they all look alike: a rounded box with a thin border (no raised edge),
// glass where the display blurs pixels (frosted_popups) and the picker colour elsewhere. A dialog makes its plane
// with create(), calls style() on every layout, and writes with ink(); a scrolling body over the box is a
// ScrollBody, which keeps the glass showing through its lines.
namespace cellui::dialog {

// The box's rounded corners and outline, from the theme.
inline void style(Plane & plane) {
  plane.setCornerRadius(plane.getStyles().theme.dialog_radius_px);
  plane.setPanelBorder();
}

// A dialog's box on `parent`, floating at `elevation` (the theme's modal height for a dialog over a scrim).
inline std::unique_ptr<Plane> create(Plane & parent, float elevation) {
  auto plane = parent.createChild();
  plane->setMouseCursor(MouseCursor::DEFAULT);
  plane->setElevation(elevation);
  style(*plane);
  return plane;
}

// What is written next on the box: `foreground` on the glass, or on the picker colour where there is none. An
// opaque part of the dialog (an input field, a button) sets its own colours with setSeeThrough(false) first, or
// calls Plane::clearFrosted().
inline void ink(Plane & plane, const Color & foreground) { plane.setInkOnGlass(foreground, plane.getStyles().picker_bg_color); }

}  // namespace cellui::dialog
