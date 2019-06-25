ConvertHGR - A simple image converter for Apple2 HGR Mode
---------------------------------------------------------

Really basic - launch it, then either click 'open' or drag and
drop your image file onto the window. It will be converted for
the Apple2's HGR Bitmap mode. There are no options for other modes.

There are a few controls available once the image is loaded:

-Nudge Right & Left - because Bitmap mode has color 
limitations for every 7 horizontal pixels, you can sometimes 
get a better result by shifting a few pixels. It never makes
sense to shift more than 7 pixels, so the program will stop
there. Usually after 4 pixels you are better off trying the
other direction. Loading a new image clears the nudge count.

-Nudge Up & Down - these shift the image up and down one pixel
at a time. Hold Shift for two pixels at a time, hold control for
4 at a time, and hold shift and control together for 8 at a time.
Up and Down refers to the virtual 'camera' direction and only
applies if the image is larger than the display due to one of 
the 'Fill Mode's in the options being set. Scrolling farther
than is possible will simply make it stop moving - the counter
will still change. You can see this in the output text window.
Loading a new image clears the nudge count.

-Stretch Hist (Hist) - this stretches the histogram of the 
image to improve contrast and enhance colors. It can also cause 
a pretty severe color shift, but in some cases it helps the 
detail of the image.

-Use Perceptual Color matching (Perc) - this changes the color
matching system to favour a model of human color visibility,
giving green the most importance, red second, and blue third.
This can often improve an image and provide more pleasing 
results, although it can sometimes result in more color shift.

-Greyscale - converts the image to greyscale on load, before any
filtering or conversion takes place. The limited Apple2 palette
is often able to render scenes more pleasingly in this mode.

-Green View - converts the image to a representation of how it
may look on a monochrome monitor, with the color removed. Note
that half-pixel representations are not displayed, but will be
saved correctly. It does not matter if you save in color or green
mode, the same data will be written. Restore the color view
with the Reload button.

-Dithering - This system uses dithering to improve the apparent 
color accuracy of the image. You can change the error distribution 
in the dialog. Each value indicates the ratio, out of 16, of the 
error that goes to that pixel. In addition, three buttons set
recommended defaults:

Floyd-Stein - sets Floyd-Steinberg dithering
Pattern - sets a regular ordered dither (every other pixel, old Ordered)
Order1 - true ordered dither with 2x2 matrix and offset adjust (use the slider)
Order2 - ordered dither with 2x2 matrix and error distribution
Order3 - true ordered dither with 4x4 matrix and offset adjust (use the slider)
Order4 - ordered dither with 4x4 matrix and error distribution
No Dither - disables dithering

Note that if the total of all values entered is less than 16,
some of the error will be lost (sometimes you may wish this!). 
If the value is more than 16, errors will accumulate and may 
induce artifacts.

The 'error mode' has two values that control how the software
deals with error correction distributed from multiple pixels.
In most cases, a pixel will receive error distribution data from
three other pixels. 'Average' mode (the default) will use the
average error when calculating a new pixel. Accumulate is the
old mode, and lets the error total up when calculating a new
pixel. Average is arguably more correct but sometimes Accumulate
gives better results (although it is usually noisier).

The setting "Luma Emphasis" scales the brightness component of each
pixel by the entered amount to control the importance of brightness
versus color. If greater than 100%, then brightness is more important,
if less, then color is more important. If 100%, then both are equally
important.

Max Color Shift controls how far each color is allowed to shift
towards a legal Apple2 HGR color BEFORE dithering takes place. This
reduces the amount of dithering at the expense of a more pronounced
color shift. Values greater than 10 or 15% will generally result in
a full color remap with little to no dithering, so keep it low!

You can set the ratios used for the Perceptual filter. Set a value
from 0-100 percent for each of the red, green and blue channels.
Ideally they should total 100, otherwise the image will definately
change. A lower value indicates that the quality of the match is
less important (allowing the color to substitute for others). A
higher value means the quality of the match is more important -
in non-perceptual mode all three have a setting of 33.3%. The default
settings are based on the scale traditionally used for converting RGB 
to luminence, with slight adjustment to taste.

When not using Perceptual mode, you can set a Luma Emphasis, this controls
how much more important brightness changes are than color changes.
Lower values tend to wash out the image, higher values tend to create
more noise.

The 'Gamma' setting applies a Gamma correction to the original image.
The value entered here is treated as a percentage, so 100 is equal to
a gamma of 1.0 (no correction). Higher values will brighten dark areas,
lower values will increase shadows.

This dialog also allows you to choose between various
resizing filters, the default is bilinear.

It also has a dropdown for a 'fill mode'. This affects images
which are not in a 4:3 ratio. In Full mode (the default), the
entire image is scaled to fit. Otherwise, the image is scaled
so that the smaller axis fits the screen, and the rest is
cropped off based on your setting, which can be top/left,
middle, or bottom/right (the setting is what part is kept).

-Open - opens a new file. Many standard image types
are supported. Files which are already in Apple2 format
are not supported at this time.

-Save Pic - saves the picture either as a memory dump (.HGR)
or as a new DOS 3.3 compatible DSK image (.DSK). In the case
of the DSK image, a bootable DOS3.3 disk will be written with
the file named "HGRFILE", and a small HELLO program to display
it. Note that the disk format is fixed and the HGRFILE will
always be stored starting on track 18, sector 0.

Note that the image is briefly displayed in green-mode during
saving, then reloaded afterwards.

-Reload - most configuration and setting changes do not re-render
the image, because of how long this takes. Press Reload to reload and
re-render the image. If you try to save without pressing Reload,
the system will remind you.

A Special mode also exists where you can randomly
browse an entire folder (this was me being silly). 
Double click anywhere on the dialog where there is no
button, and 'Open' will become 'Next'. Click it, and
select a file in a folder you would like to browse.
All images in that folder (and all subfolders) will
be available in random order each time you click 'Next'.
You can process and save any you like. To exit this
mode you must close the program.

Finally, there is a simple command line interface.
Passing one filename will cause that image to be loaded and
displayed in the GUI automatically.
Passing a second makes the result automatically be saved to
that filename without displaying the GUI. Two files will be
saved, an HGR (memory dump) file, and a BMP showing the result.

Due to the large number of options and the differences between
images, getting pleasing output is a bit of an art. Try playing
with one option at a time, reloading each change, to get a feel
for how each affects the image.

This program uses the ImgSource Library by Smaller Animals Software.

//
// (C) 2013 Mike Brent aka Tursi aka HarmlessLion.com
// This software is provided AS-IS. No warranty
// express or implied is provided.
//
// This notice defines the entire license for this software.
// All rights not explicity granted here are reserved by the
// author.
//
// You may redistribute this software provided the original
// archive is UNCHANGED and a link back to my web page,
// http://harmlesslion.com, is provided as the author's site.
// It is acceptable to link directly to a subpage at harmlesslion.com
// provided that page offers a URL for that purpose
//
// Source code, if available, is provided for educational purposes
// only. You are welcome to read it, learn from it, mock
// it, and hack it up - for your own use only.
//
// Please contact me before distributing derived works or
// ports so that we may work out terms. I don't mind people
// using my code but it's been outright stolen before. In all
// cases the code must maintain credit to the original author(s).
//
// Unless you have explicit written permission from me in advance,
// this code may never be used in any situation that changes these
// license terms. For instance, you may never include GPL code in
// this project because that will change all the code to be GPL.
// You may not remove these terms or any part of this comment
// block or text file from any derived work.
//
// -COMMERCIAL USE- Contact me first. I didn't make
// any money off it - why should you? ;) If you just learned
// something from this, then go ahead. If you just pinched
// a routine or two, let me know, I'll probably just ask
// for credit. If you want to derive a commercial tool
// or use large portions, we need to talk. ;)
//
// Commercial use means ANY distribution for payment, whether or
// not for profit.
//
// If this, itself, is a derived work from someone else's code,
// then their original copyrights and licenses are left intact
// and in full force.
//
// http://harmlesslion.com - visit the web page for contact info
//

