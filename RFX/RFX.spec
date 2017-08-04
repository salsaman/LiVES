The RFX (Rendered Effects System)
---------------------------------

Author: salsaman@gmail.com
Date: 3/3/2013
API Version: 1.8 GPL

Changes
1.0 First version salsaman@xs4all.nl
1.1 Added input channels
1.2 Added "<author>" section
1.3 Added "<define>" section
1.4 Added string support, made line delimiter fixed (\n)
1.5 Added note about LC_NUMERIC
d1.6 Added string_list parameter type
1.7 Updates for compatibility with realtime effects
1.8 Add special multirect, deprecate multrect, and remove audiochannel.

--- API Version frozen ---

- Add "special|framedraw|multirect|"; add some hints about interpreting "special" keyword in "<layout>"
- Add "special|framedraw|singlepoint|"
- multirect -> multrect, fix description of it.
- Add missing "label" field for string (documentation error)
- Correct "language code"
- Added "batch generator" flag bit + explanation of generators.
- Describe how width and height are passed back to host.
- Add "special|fileread"
- Added note about differing input image types
- Add "special|password"
- Fix text errors, add note about "$fps"
- Clarify max length for fileread special keyword.
- tighten up wording a little bit

TODO: 	- split into RFX layout and RFX plugin components (?)






Note:
This license documents a standard. The license of this document is the GNU FDL (GNU Free Documentation License) version 1.3 or higher. 
The standard itself is released under the GPL v3 or higher.

See: http://www.gnu.org/copyleft/fdl.html
     http://www.gnu.org/licenses/gpl-3.0.html



1. Introduction
     
RFX is a system for generating parameter windows and plugins, e.g. rendered 
effects from scripts. It currently has features for:

- defining parameters
- sending layout hints to a GUI about how to present a parameter window
- allowing triggers when the parameter window is initialised, and/or when any 
     of the parameter values are altered
- allowing definition of code for pre/loop/post processing (e.g. of blocks of frames)


----------------------------------------------------------------------


Note: RFX layout is now intended to be a standalone component which can be used for passing parameter window descriptions 
between applications. It consists only of the sections <define>, <params>, <language_code> (and optionally <param_window> and <onchange>). 
The <name> section is also required, but can be generated ad-hoc by the host.




--------------------------------------------------------------------



2. RFX Scripts


The following section describes a full RFX script, and the rules which must be 
implemented in order to comply with the RFX script version 1.8


NOTE: for RFX, the shell variable LC_NUMERIC should be set to "C"; that implies the radix character for 
numeric values is "."


Scripts
------------

Scripts are the beginning point for all effects/tools/utilities in RFX.
From the script, an interface and or plugin can be generated for a particular host 
application. The details of the plugin generation are left to the authors of 
the relevant host applications. RFX script files are abstracted in the sense 
that they are not tied to any particular widget set, or to any particular 
method of transferring data between host and plugin.

The function of the plugin is to return the data in each section as requested by the host. If applicable, it
should also execute the code in the relevant code sections <onchange>, <pre>, <post>, <loop>. One may think of a builder tool which
takes a script and generates a plugin, which the host can then load and use. For example there is the build_lives_rfx_plugin tool for the LiVES video editor
which can generate an executable Perl plugin from a full RFX script.

The function of the host is build an interface as defined by either the parameters, parameter_window sections, or by combining both.
The host should then run this parameter window and marshall the parameters back to the plugin.

Some RFX scripts may use only the parameter_window and the other mandatory sections.




Here are the sections (some mandatory, some optional) which comprise an RFX 
script file. Each section in the script file is laid out as follows:

<keyword>
value1
value2
</keyword>

some keyword sections are optional, these are indicated below like:
<keyword> [optional]

some keyword sections take a single data value, some take multiple data values
Keywords which take multiple values are indicated thus:
<keyword> [list]

In the case of multiple values, each value should be terminated by a newline 
(\n) character.

Within values, the delimiter is '|'. A trailing delimiter in a line is generally ignored, but can be added 
for readability (as in some of the examples below).

Note:
As of RFX version 1.4, the field delimiter is set in the <define>
section. However, the default value is assumed below.

The field delimiter must be a single (ASCII) character. It may not be \0 or \n.



Here are the RFX sections:

<define>

<name>

<version> [plugins only]

<author> [plugins only]

<description> [plugins only]

<requires> [optional] [plugins only] [list]

<params> [optional] [list]

<param_window> [optional] [list]

<properties> [plugins only] [optional]

<language_code>

<pre> [plugins only] [optional] [list]

<loop> [plugins only] [optional] [list]

<post> [plugins only] [optional] [list]

<onchange> [optional] [list]

The contents of each section are described below.


<define>

This section contains the following entities:
field delimiter
RFX version

E.g.:

<define>
|1.8
</define>

As noted, the following assumes the default field delimiter of |.



<name>

This section contains a single entry with the name of the effect. The name 
not contain spaces, newlines or the delimiter character (|). Each plugin name should be unique for that author.


e.g.

<name>
blank_frames
</name>




<version>

This is a mandatory section for a version string for a plugin. It should be an integer. 
Do not confuse this with the RFX version in the <define> section.

e.g.

<version>
1
</version>

<author>

This is a mandatory section for an "author" string for a plugin. 
Any format is acceptible, but it should not contain newlines or the 
delimiter character. An optional second field may contain the author's URL.

e.g.

<author>
somebody@somewhere.com|http://www.mysite.org|
</author>



<description>

This section consists of a single entry with 4 fields, it is mandatory for a plugin:

menu text|action description|minimum frames|num_channels

e.g.

<description>
Edge Detect|Edge detecting|1|1
</description>

minimum frames is the minimum block length for frames that the plugin will 
accept. A value of 0 indicates a minimum of 1 frame (i.e. 0 is equivalent to 1). If num_channels is 0 (a generator), this value is ignored, (unless it is -1, see below).

num_channels indicates the number of input channels. (LiVES currently will only 
accept 0,1, or 2 input channels).


IMPORTANT NOTE
A value of -1 for minimum frames indicates that the plugin will not actually 
process frames. This is useful for plugins where only a parameter window is 
needed. In the case that minimum frames is -1, the host application should 
simply display the parameter window and allow the user to change the values, 
but no processing of frames should be performed. This is useful with trigger 
code for tools like calculators.




<requires> [optional] [list]

This section consists of a list of required binaries for the plugin to run. 
Each entry in the list should be separated by a newline.

e.g.

<requires>
convert
composite
</requires>

The host and/or plugin will check that the required binaries are present in the user's $PATH
before attempting to process any frames.




<params> [optional] [list]

This section contains a list of parameters for the effect. Each entry in the 
list should be separated by a newline. Each entry in the list contains a 
variable number of fields depending on the parameter Type.

The name of each parameter should be unique within the plugin.


The types which are defined so far are:

num
bool
colRGB24
string
string_list

Each type is defined thus:

num: this is a numeric value. The 'num' type is immediately followed by a 
decimal number indicating the number of decimal places. An integer value is 
therefore declared as 'num0', while a numerical value with 6 decimal places of 
accuracy is declared as 'num6'.

The parameter format for a num is as follows:
parameter_name|label|type|default|minimum|maximum|step_size/wrap|


IMPORTANT NOTE
An underscore in the label indicates that the following character is a
mnemonic accelerator for the parameter. In case the host does not support 
mnemonic accelerators, underscores should be stripped from the label.


The step size is the amount that the value is adjusted by when the arrows are clicked on a spinbutton representing this parameter.
A negative step_size means that the parameter values should wrap (max to min and min to max).

e.g.

<params>
rotate|_Rotate selection by|num0|360|-3600|3600|
</params>



As noted earlier, LC_NUMERIC should be set to "C", so a "." should be used to represent a decimal point.


bool: this indicates a boolean parameter
The format for this parameter type is:

parameter_name|label|type|default|group|

e.g.

<params>
shrink|_Shrink rotated window to fit frame size|bool|0|0|
</params>

The default is constrained to the values 0 (FALSE) and 1 (TRUE).

The group field is an optional integer. If it is non-zero then this indicates a radio button group. That is, for all boolean parameters in the 
plugin which have the same group, only one may take a value of 1 (TRUE). 





colRGB24: this indicates a 24 bit RGB value. The leftmost 8 bits correspond to 
red, the next 8 bits to green, and the rightmost 8 bits to blue.

The format for this parameter type is:
parameter_name|label|type|default_red|default_green|default_blue|

e.g.
<params>
startcol|_Start colour|colRGB24|255|255|255|
</params>


string: this indicates a string parameter
The format for this parameter type is:

parameter_name|label|type|default|max_length_in_utf-8_chars|

The default may not contain the field delimiter character. The encoding used is UTF-8.


max_length is measured in utf-8 characters. It is an optional field, and if omitted, the maximum string length for the "language_code" will be used.
e.g.

<params>
text|Enter some _text|string|The default value is "default".\nThis is line 2|1024|
</params>



string_list: this indicates a list of string parameters, from which exactly one entry may be selected
The format for this parameter type is:

parameter_name|label|type|default|first_string|second_string|...

No string may contain the field delimiter character. Newline may be represented by \n or 
whatever the locale equivalent of \n is.



The default is an index into the list of strings. 0 means the first string, 1 means the second, etc.
The value returned will be an integer.

e.g.

<params>
method|Choose a _method|string_list|0|method1|method2|method3|
</params>


Because of the ambiguity with trailing field delimeters, the last string in the list may not be empty.
Empty strings are however, allowed at all other positions in the list.

By default, no checking is done to make sure that the current value is contained in the list, 
the list is simply a guide to some subset of possible values.








RFX Interface Section
---------------------



<param_window> [optional] [list]

This section is used to provide layout hints to the host about how to draw a 
parameter window for this effect/tool.

The format is a list of layout hints separated by newlines. Each line in the 
list has a keyword followed by one or more subfields separated by "|"

Two keywords are currently supported:

layout
special


layout is hint to the host about how to lay out the parameter window. It is 
abstracted from any particular widget set. 

It is up to the host how each layout line in this section is actually 
interpreted.
 
One way to implement this is to assume each layout line describes a horizontal 
box. These horizontal boxes can be laid out within a vertical container. Any 
unassigned parameters can be added below by the host.

The layout keyword is followed by 0 or more fields which describe a layout row.
the fields can be:

px == position of parameter x on the screen. The first parameter is p0, the second is p1, etc. The order of parameter numbering is taken from the
<parameter> section.

fill == fill with blank space
"label" == a label
hseparator == horizontal line separator


e.g:

<param_window>
layout|p0|"this is a label"|
layout|p5|
layout|p1|fill|p2|
layout|hseparator|
layout|p3|fill|
layout|p4|fill|
</param_window>



A second keyword which can be used in the param_window section is "special".
This indicates that the host can optionally add a widget to the window which 
links together some of the parameters in a special way, or has some variant functionality.

Two examples are:

special|aspect|1|2
special|framedraw|rectdemask|1|2|3|4

It is up to individual host authors how they visually interpret the special keyword.
For benefit of hosts, only one special widget with "framedraw" type should be used per plugin.


Special widgets
---------------
Suggested interpretation:

  If multi-valued parameters are allowed, each linked parameter can have n values, 
  where n matches the current number of in_channels (including "disabled" channels) to the filter
  In this case it is up to the host how the currently selected index [n] is chosen.


Special type "framedraw" - allows user drawing on a preview frame to control some parameters and vice-versa; origin is top-left

	If a linked parameter has 0 decimal places, then the value is in pixels. If more than 0 decimal places, the value is a ratio 
	where {1.0,1.0} is the bottom right of the image [as of version 1.8]

  Subtype "rectdemask" - >= 4 numeric parameters : demask (only show) a region on the preview frame, from position p0[n],p1[n] to 
                                           p2[n],p3[n]

  Subtype "multrect" - >= 4 numeric parameters : draw a rectangle (outline) on preview frame, offset p0[n]*out_channel1_width,p1[n]*out_channel1_height, scaled by p2[n]*out_channel_width,p3[n]*out_channel_width; [deprecated as of version 1.8]

  Subtype "multirect" - >= 4 numeric parameters : draw a rectangle (outline) on preview frame, position p0[n],p1[n], to position p2[n],p3[n]

  Subtype "singlepoint" - >= 2 numeric parameters : draw a "target" point (e.g. crosshair) on the frame, at offset 
                                            p0[n],p1[n]


Special type "aspect" - 2 numeric parameters : p0 and p1 may optionally be aspect ratio locked to the corresponding in channel 
                                       width and height; host should provide a way to select/deselect this

Special type "mergealign" – 2 parameters
The mergealign special widget links together two num type parameters. It has two states. In one state it will set the first parameter to its minimum value and the second parameter to its maximum value. In the second state it will do the opposite, set the first parameter to its maximum, and the second parameter to its minimum.

Special type "fileread" - 1 string parameter : the linked string parameter should point to a file to which the user has read permissions. In this case the maximum string length may be ignored by the host to avoid truncating longer filenames.

Special type "password" - 1 string parameter : the host may hide/obscure the input to this string




================================== end interface section ========================================================






<properties> [optional]

this is a bitmap field (currently 32 bit). Hexadecimal values are allowed.
some bits are defined already

0x0001 == slow (hint to host)
0x0002 == may resize
0x0004 == batch mode generator

If the "may resize" bit is set, _all_ frames in the processing block may be 
resized to a new width and height. The host should take measures to determine the new width / height of the output using its
standard discovery methods.


A "batch mode" generator is a plugin with 0 in channels, and which generates all frames in a single pass (i.e. the loop code is only to be run once).



0x8000 == reserved, may be ignored / set / reset.
0x4000 == reserved, may be ignored / set / reset.
0x2000 == reserved, may be ignored / set / reset.
0x1000 == reserved, may be ignored / set / reset.

If the value of properties is not defined, it is assumed to be 0x0000.



e.g.
<properties>
0x0001
</properties>




<language_code>

The language code indicates the scripting language for the <pre>, <loop>, 
<post> and <onchange> sections. This is a 32 bit unsigned integer. Hexadecimal values are allowed.

Current values are:

0xF0 == LiVES-perl




<pre> [optional] [list]

This section contains code which will be executed before the processing loop.

e.g. (in LiVES-perl):

<pre>
#calculate the number of frames
$length=$end-$start+1;
</pre>


For non-batch-mode generators using LiVES-perl, this section is mandatory, and you must set $end equal to the number of frames which will be generated, e.g.:

<pre>
$end=100;
</pre>

For batch mode or non-batch-mode generators, you can also set the frame rate for the clip (this is optional). In LiVES-perl, this is done by setting $fps (which can take a floating point value), e.g.

<pre>
$end=100;
$fps=25.0;
</pre>




<loop> [list]

IMPORTANT NOTE
This section is optional for non-processing scripts (where min frames is set 
to -1, see above)

This section contains code which will be executed for each frame in the 
processing block.

e.g. (in LiVES-perl):

<loop>
#negate each frame
system("$convert_command -negate $in $out");
</loop>


The rules for processing are as follows:

in each pass of the loop you should produce an output frame (possibly by using 
the input frame). For example, in LiVES-perl, the output frame is called $out.
The output frames should be created in numerical from the start of the 
processing block to the end in numerical order with no gaps. The image type 
should not be changed, nor should the palette. Frames may be resized provided 
the script has the "may resize" property bit set.

If no processing is to be done on a pass, then the input frame should be 
copied to the output frame.

An exception to this is if the plugin has no in channels, and set the "batch generator" flag bit. In this case, the loop code will be run *only once*, and the generator should create an alpha-numeric sequence of image files. If the plugin is a generator, and this flag bit is not set, the plugin should generate frame $out, and it should be of the correct type (e.g. jpeg if $out ends in .jpg) and all frames must be of the same width and height.

One further exception - if two or more input frames are to be merged into one output frame, the 
input frames may be of different types (for example, a png image and a jpeg image may be merged 
into a single png image). In this case, all input images should be converted to the first input 
image type before merging. The resulting output image should also be of the same type.



If the plugin sets the may-resize bit, or is a non-batch-mode generator, the width and height of the frames should be passed back to the host.
In LiVES-perl this is done by setting $nwidth and $nheight to the width and height in pixels, respectively.



Plugins with no in channels (generators) may also set $fps to inform the host of the framerate of the 
generated clip.




<post> [optional] [list]

This section contains code which should be executed after all frames in the 
block have been processed.

e.g. (in LiVES-perl):

<post>
#clean up a temporary file
unlink "blank.jpg";
</post>




<onchange> [optional] [list]

This section contains the triggers for changing parameters either when the 
parameter window is first created, or when a parameter is changed. For example
 an init type trigger could set the maximum and minimum values for a parameter 
depending on the current frame width and height.

The format is:

event|code

event can be either "init" or an integer representing a parameter. The first 
parameter is given the number 0, the second parameter 1, etc.
There can be multiple lines of code for each type of change. Code for the same 
trigger type is executed in the order in which it appears in this section. The format of the code depends on the value of <language_code>.


e.g. (in LiVES-perl):

<onchange>
init|$p0_min=-$width+1;$p0_max=$width-1;$p1_min=-$height+1;$p1_max=$height-1;
0|if ($p0) {$p3=$p4*$p5;} elsif ($p1&&$p5>0.) {$p4=$p3/$p5;} elsif ($p4>0.) {$p5=$p3/$p4;}
</onchange>



