/*!
 * \file src/hid/fltk/drawing_area.cpp
 *
 * \author Bert Timmerman <bert.timmerman@xs4all.nl>
 *
 * \copyright (C) 2023 PCB Contributors.
 *
 * <hr>
 *
 * <h1><b>Copyright.</b></h1>\n
 *
 * PCB, interactive printed circuit board design
 *
 * Copyright (C) 1994,1995,1996, 2004 Thomas Nau
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Contact addresses for paper mail and Email:
 * Thomas Nau, Schlehenweg 15, 88471 Baustetten, Germany
 * Thomas.Nau@rz.uni-ulm.de
 */


/* Standard headers */


/* Fltk headers */
#include "FLTK_headers.h"


#include "drawing_area.h"


/*!
 * \brief Constructor for the drawing area.
 */
DrawingArea::DrawingArea (int x, int y, int w, int h) : Fl_Gl_Window _window (0, 0, 100, 50)
(
  _dx (x),
  _dy (y),
  _width (w),
  _height (h),
  /*! \todo Add code here. */
)
{
}


/*!
 * \brief Destructor for the drawing area.
 */
DrawingArea::~DrawingArea ()
{
  /*! \todo Add code here. */
}


/*!
 * \brief Show the drawing area.
 */
void
DrawingArea::show (void)
{
  /*! \todo Add code here. */
}


/*!
 * \brief Hide the drawing area.
 */
void
DrawingArea::hide (void)
{
  /*! \todo Add code here. */
}


/*!
 * \brief Draw on the drawing area.
 */
void
DrawingArea::draw (void)
{
  if (!valid ())
  {
    /*
     * set up projection, viewport, etc.
     * window size is in w() and h().
     * valid () is turned on by FLTK after draw () returns.
     */
  }

  /* ... draw stuff ... */
}


/*!
 * \brief Handle events.
 */
int
DrawingArea::handle (int event)
{
  switch (event)
  {
    case FL_PUSH:
      //... mouse down event.
      //... position in Fl::event_x() and Fl::event_y().
      return 1;

    case FL_DRAG:
      //... mouse moved while down event.
      return 1;

    case FL_RELEASE:   
      //... mouse up event.
      return 1;

    case FL_FOCUS :
      //... Return 1 if you want keyboard events, 0 otherwise.
      return 1;

    case FL_UNFOCUS :
      //... Return 1 if you want keyboard events, 0 otherwise.
      return 1;

    case FL_KEYBOARD:
      //... keypress, key is in Fl::event_key(), ascii in Fl::event_text().
      //... Return 1 if you understand/use the keyboard event, 0 otherwise.
      return 1;

    case FL_SHORTCUT:
      //... shortcut, key is in Fl::event_key(), ascii in Fl::event_text().
      //... Return 1 if you understand/use the shortcut event, 0 otherwise.
      return 1;

    default:
      // pass other events to the base class.
      return Fl_Gl_Window::handle (event);
  }
}
