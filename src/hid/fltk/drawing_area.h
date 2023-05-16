/*!
 * \file src/hid/fltk/drawing_area.h
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


#ifndef PCB_SRC_HID_FLTK_DRAWING_AREA_H__
#define PCB_SRC_HID_FLTK_DRAWING_AREA_H__


/* Standard headers */
#include <string>


#pragma warning (push, 0)


/* Fltk headers */
#include "FLTK_headers.h"
#include <FL/gl.h>
#include <GL/glu.h>


#pragma warning (pop)


/*!
 * \class Drwaing areaCanvas.
 * 
 * \brief Create a drawing area for pcb art work.
 *
 * 
*/
class DrawingArea : Fl_Gl_Window _window
{
  private:
    int _dx;
    int _dy;
    int _width;
    int _height;
    Fl_Gl_Window *_window;

  public:
    DrawingArea (int x, int y, int w, int h); /*!< Constructor. */
    ~DrawingArea (); /*!< Destructor. */
    void show (void); /*!< Show the drawing area. */
    void hide (void); /*!< Hide the drawing area. */
    void draw (void); /*!< Draw on the drawing area. */
    int handle (int event); /*!< Handle events. */

  private:
};


#endif /* PCB_SRC_HID_FLTK_DRAWING_AREA_H__ */


/* EOF */
