/* GemRB - Infinity Engine Emulator
 * Copyright (C) 2003 The GemRB Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *
 */

#include "GUI/Window.h"

#include "GUI/Button.h"
#include "GUI/MapControl.h"
#include "GUI/ScrollBar.h"

#include "win32def.h"
#include "GameData.h"
#include "ImageMgr.h"
#include "ie_cursors.h"

namespace GemRB {

Window::Window(const Region& frame, WindowManager& mgr)
	: View(frame), manager(mgr)
{
	visibility = Window::INVISIBLE;
	Cursor = IE_CURSOR_NORMAL;

	focusView = NULL;
	trackingView = NULL;
	hoverView = NULL;

	lastMouseMoveTime = GetTickCount();
}

Window::~Window()
{

}

void Window::Close()
{
	manager.CloseWindow(this);
}

bool Window::DisplayModal(WindowManager::ModalShadow shadow)
{
	return manager.MakeModal(this, shadow);
}

/** Add a Control in the Window */
void Window::SubviewAdded(View* view, View* parent)
{
	Control* ctrl = dynamic_cast<Control*>(view);
	if (ctrl) {
		if (ctrl->Owner == this) return; // already added!
		ctrl->Owner = this;
		if (ctrl->ControlType == IE_GUI_SCROLLBAR && parent == this) {
			scrollbar = static_cast<ScrollBar*>(ctrl);
		}
		Controls.push_back( ctrl );
	}
}

void Window::SubviewRemoved(View* subview, View* /*parent*/)
{
	Control* ctrl = dynamic_cast<Control*>(subview);
	if (ctrl) {
		ctrl->Owner = NULL;
		std::vector< Control*>::iterator it = std::find(Controls.begin(), Controls.end(), ctrl);
		if (it != Controls.end())
			Controls.erase(it);
	}
	if (focusView == ctrl) {
		focusView = NULL;
	}
}

void Window::Focus()
{
	manager.FocusWindow(this);
}

void Window::SetFocused(Control* ctrl)
{
	if (ctrl) {
		TrySetFocus(ctrl);
	} else if (Controls.size()) {
		// set a default focus, something should always be focused
		TrySetFocus(Controls[0]);
	}
}

void Window::SetPosition(WindowPosition pos)
{
	// start at top left
	Region newFrame(Point(), frame.Dimensions());
	Size screen = manager.ScreenSize();

	// adjust horizontal
	if (pos&PosHmid) {
		newFrame.x = (screen.w / 2) - (newFrame.w) / 2;
	} else if (pos&PosRight) {
		newFrame.x = screen.w - newFrame.w;
	}

	// adjust vertical
	if (pos&PosVmid) {
		newFrame.y = (screen.h / 2) - (newFrame.h) / 2;
	} else if (pos&PosBottom) {
		newFrame.y = screen.h - newFrame.h;
	}
	SetFrame(newFrame);
}

void Window::SetVisibility(Visibility vis)
{
	if (vis == visibility) return;

	visibility = vis;
	MarkDirty();
}

/** This function Draws the Window on the Output Screen */
void Window::DrawSelf(Region /*drawFrame*/, const Region& /*clip*/)
{
	if (visibility <= INVISIBLE) return; // no point in drawing invisible windows

	if (!(flags&WF_BORDERLESS) && (frame.w < core->Width || frame.h < core->Height)) {
		Video* video = core->GetVideoDriver();
		video->SetScreenClip( NULL );

		Sprite2D* edge = WinFrameEdge(0); // left
		video->BlitSprite(edge, 0, 0, true);
		edge = WinFrameEdge(1); // right
		int sideW = edge->Width;
		video->BlitSprite(edge, core->Width - sideW, 0, true);
		edge = WinFrameEdge(2); // top
		video->BlitSprite(edge, sideW, 0, true);
		edge = WinFrameEdge(3); // bottom
		video->BlitSprite(edge, sideW, core->Height - edge->Height, true);
	}
}

Control* Window::GetFocus() const
{
	return dynamic_cast<Control*>(FocusedView());
}

void Window::RedrawControls(const char* VarName, unsigned int Sum)
{
	for (std::vector<Control *>::iterator c = Controls.begin(); c != Controls.end(); ++c) {
		Control* ctrl = *c;
		ctrl->UpdateState( VarName, Sum);
	}
}

bool Window::TrySetFocus(View* target)
{
	if (target && !target->CanLockFocus()) {
		// target wont accept focus so dont bother unfocusing current
		return false;
	}
	if (focusView && !focusView->CanUnlockFocus()) {
		// current focus unwilling to reliquish
		return false;
	}
	focusView = target;
	return true;
}

void Window::DispatchMouseOver(const Point& p)
{
	TooltipTime = GetTickCount() + ToolTipDelay;

	// need screen coordinates because the target may not be a direct subview
	Point screenP = ConvertPointToScreen(p);
	View* target = SubviewAt(p, false, true);
	bool left = false;
	if (target) {
		if (target != hoverView) {
			if (hoverView) {
				hoverView->OnMouseLeave(hoverView->ConvertPointFromScreen(screenP), drag.get());
				left = true;
			}
			target->OnMouseEnter(target->ConvertPointFromScreen(screenP), drag.get());
		}
	} else if (hoverView) {
		hoverView->OnMouseLeave(hoverView->ConvertPointFromScreen(screenP), drag.get());
		left = true;
	}
	if (left) {
		assert(hoverView);
		if (trackingView && !drag) {
			drag = trackingView->DragOperation();
		}
		if (trackingView == hoverView
			&& !trackingView->TracksMouseDown())
		{
			trackingView = NULL;
		}
	}
	if (trackingView) {
		// tracking will eat this event
		trackingView->OnMouseOver(trackingView->ConvertPointFromScreen(screenP));
	} else if (target) {
		target->OnMouseOver(target->ConvertPointFromScreen(screenP));
	}
	hoverView = target;
	TooltipView = hoverView;
}

void Window::DispatchMouseDown(const Point& p, unsigned short button, unsigned short mod)
{
	View* target = SubviewAt(p, false, true);
	if (target) {
		TrySetFocus(target);
		Point subP = target->ConvertPointFromScreen(ConvertPointToScreen(p));
		target->OnMouseDown(subP, button, mod);
		trackingView = target; // all views track the mouse within their bounds
		return;
	}
	// handle scrollbar events
	View::OnMouseDown(p, button, mod);
}

void Window::DispatchMouseUp(const Point& p, unsigned short button, unsigned short mod)
{
	if (trackingView) {
		Point subP = trackingView->ConvertPointFromScreen(ConvertPointToScreen(p));
		trackingView->OnMouseUp(subP, button, mod);
	} else if (drag) {
		View* target = SubviewAt(p, false, true);
		if (target && target->AcceptsDragOperation(*drag)) {
			target->CompleteDragOperation(*drag);
		}
	}
	drag = NULL;
	trackingView = NULL;
}

void Window::DispatchMouseWheelScroll(short x, short y)
{
	Point mp = core->GetVideoDriver()->GetMousePos();
	View* target = SubviewAt(ConvertPointFromScreen(mp), false, true);
	if (target) {
		target->OnMouseWheelScroll( x, y );
		return;
	}
	// handle scrollbar events
	View::OnMouseWheelScroll(x, y);
}

bool Window::OnSpecialKeyPress(unsigned char key)
{
	bool handled = false;
	if (key == GEM_TAB && hoverView) {
		// tooltip maybe
		handled = hoverView->View::OnSpecialKeyPress(key);
	} else if (focusView) {
		handled = focusView->OnSpecialKeyPress(key);
	}

	Control* ctrl = NULL;
	//the default control will get only GEM_RETURN
	if (key == GEM_RETURN) {
		//ctrl = GetDefaultControl(0);
	}
	//the default cancel control will get only GEM_ESCAPE
	else if (key == GEM_ESCAPE) {
		//ctrl = GetDefaultControl(1);
	} else if (key >= GEM_FUNCTION1 && key <= GEM_FUNCTION16) {
		// TODO: implement hotkeys
	} else {
		ctrl = dynamic_cast<Control*>(FocusedView());
	}

	if (ctrl) {
		switch (ctrl->ControlType) {
				//scrollbars will receive only mousewheel events
			case IE_GUI_SCROLLBAR:
				if (key != GEM_UP && key != GEM_DOWN) {
					return false;
				}
				break;
				//buttons will receive only GEM_RETURN
			case IE_GUI_BUTTON:
				if (key >= GEM_FUNCTION1 && key <= GEM_FUNCTION16) {
					//fake mouse button
					ctrl->OnMouseDown(Point(), GEM_MB_ACTION, 0);
					ctrl->OnMouseUp(Point(), GEM_MB_ACTION, 0);
					return false;
				}
				if (key != GEM_RETURN && key!=GEM_ESCAPE) {
					return false;
				}
				break;
				// shouldnt be any harm in sending these events to any control
		}
		ctrl->OnSpecialKeyPress( key );
		return true;
	}
	// handle scrollbar events
	return View::OnSpecialKeyPress(key);
}

Sprite2D* Window::WinFrameEdge(int edge)
{
	std::string refstr = "STON";
	switch (core->Width) {
		case 800:
			refstr += "08";
		break;
		case 1024:
			refstr += "10";
		break;
	}
	switch (edge) {
		case 0:
			refstr += "L";
			break;
		case 1:
			refstr += "R";
			break;
		case 2:
			refstr += "T";
			break;
		case 3:
			refstr += "B";
			break;
	}

	typedef Holder<Sprite2D> FrameImage;
	static std::map<ResRef, FrameImage> frames;

	ResRef ref = refstr.c_str();
	Sprite2D* frame = NULL;
	if (frames.find(ref) != frames.end()) {
		frame = frames[ref].get();
	} else {
		ResourceHolder<ImageMgr> im(ref);
		frame = im->GetSprite2D();
		frames.insert(std::make_pair(ref, frame));
	}

	return frame;
}

ViewScriptingRef* Window::MakeNewScriptingRef(ScriptingId id)
{
	WindowID = id;
	return new WindowScriptingRef(this, id);
}

}
