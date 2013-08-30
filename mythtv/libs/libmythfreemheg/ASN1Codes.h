/* ASN1Codes.h

   Copyright (C)  David C. J. Matthews 2004  dm at prolingua.co.uk

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
   Or, point your browser to http://www.gnu.org/copyleft/gpl.html

*/
#ifndef ASN1CODES_H
#define ASN1CODES_H

// Universal codes _ standard across all ASN1 definitions
#define U_BOOL      1
#define U_INT       2
#define U_STRING    4
#define U_NULL      5
#define U_ENUM      10
#define U_SEQUENCE  16

// Context_specific codes _ defined for MHEG 5
#define C_APPLICATION               0
#define C_SCENE                     1
#define C_STANDARD_IDENTIFIER       2
#define C_STANDARD_VERSION          3
#define C_OBJECT_INFORMATION        4
#define C_ON_START_UP               5
#define C_ON_CLOSE_DOWN             6
#define C_ORIGINAL_GC_PRIORITY      7
#define C_ITEMS                     8
#define C_RESIDENT_PROGRAM          9
#define C_REMOTE_PROGRAM            10
#define C_INTERCHANGED_PROGRAM      11
#define C_PALETTE                   12
#define C_FONT                      13
#define C_CURSOR_SHAPE              14
#define C_BOOLEAN_VARIABLE          15
#define C_INTEGER_VARIABLE          16
#define C_OCTET_STRING_VARIABLE     17
#define C_OBJECT_REF_VARIABLE       18
#define C_CONTENT_REF_VARIABLE      19
#define C_LINK                      20
#define C_STREAM                    21
#define C_BITMAP                    22
#define C_LINE_ART                  23
#define C_DYNAMIC_LINE_ART          24
#define C_RECTANGLE                 25
#define C_HOTSPOT                   26
#define C_SWITCH_BUTTON             27
#define C_PUSH_BUTTON               28
#define C_TEXT                      29
#define C_ENTRY_FIELD               30
#define C_HYPER_TEXT                31
#define C_SLIDER                    32
#define C_TOKEN_GROUP               33
#define C_LIST_GROUP                34
#define C_ON_SPAWN_CLOSE_DOWN       35
#define C_ON_RESTART                36
#define C_DEFAULT_ATTRIBUTES        37
#define C_CHARACTER_SET             38
#define C_BACKGROUND_COLOUR         39
#define C_TEXT_CONTENT_HOOK         40
#define C_TEXT_COLOUR               41
#define C_FONT2                     42
#define C_FONT_ATTRIBUTES           43
#define C_IP_CONTENT_HOOK           44
#define C_STREAM_CONTENT_HOOK       45
#define C_BITMAP_CONTENT_HOOK       46
#define C_LINE_ART_CONTENT_HOOK     47
#define C_BUTTON_REF_COLOUR         48
#define C_HIGHLIGHT_REF_COLOUR      49
#define C_SLIDER_REF_COLOUR         50
#define C_INPUT_EVENT_REGISTER      51
#define C_SCENE_COORDINATE_SYSTEM   52
#define C_ASPECT_RATIO              53
#define C_MOVING_CURSOR             54
#define C_NEXT_SCENES               55
#define C_INITIALLY_ACTIVE          56
#define C_CONTENT_HOOK              57
#define C_ORIGINAL_CONTENT          58
#define C_SHARED                    59
#define C_CONTENT_SIZE              60
#define C_CONTENT_CACHE_PRIORITY    61
#define C_LINK_CONDITION            62
#define C_LINK_EFFECT               63
#define C_NAME                      64
#define C_INITIALLY_AVAILABLE       65
#define C_PROGRAM_CONNECTION_TAG    66
#define C_ORIGINAL_VALUE            67
#define C_OBJECT_REFERENCE          68
#define C_CONTENT_REFERENCE         69
#define C_MOVEMENT_TABLE            70
#define C_TOKEN_GROUP_ITEMS         71
#define C_NO_TOKEN_ACTION_SLOTS     72
#define C_POSITIONS                 73
#define C_WRAP_AROUND               74
#define C_MULTIPLE_SELECTION        75
#define C_ORIGINAL_BOX_SIZE         76
#define C_ORIGINAL_POSITION         77
#define C_ORIGINAL_PALETTE_REF      78
#define C_TILING                    79
#define C_ORIGINAL_TRANSPARENCY     80
#define C_BORDERED_BOUNDING_BOX     81
#define C_ORIGINAL_LINE_WIDTH       82
#define C_ORIGINAL_LINE_STYLE       83
#define C_ORIGINAL_REF_LINE_COLOUR  84
#define C_ORIGINAL_REF_FILL_COLOUR  85
#define C_ORIGINAL_FONT             86
#define C_HORIZONTAL_JUSTIFICATION  87
#define C_VERTICAL_JUSTIFICATION    88
#define C_LINE_ORIENTATION          89
#define C_START_CORNER              90
#define C_TEXT_WRAPPING             91
#define C_MULTIPLEX                 92
#define C_STORAGE                   93
#define C_LOOPING                   94
#define C_AUDIO                     95
#define C_VIDEO                     96
#define C_RTGRAPHICS                97
#define C_COMPONENT_TAG             98
#define C_ORIGINAL_VOLUME           99
#define C_TERMINATION               100
#define C_ENGINE_RESP               101
#define C_ORIENTATION               102
#define C_MAX_VALUE                 103
#define C_MIN_VALUE                 104
#define C_INITIAL_VALUE             105
#define C_INITIAL_PORTION           106
#define C_STEP_SIZE                 107
#define C_SLIDER_STYLE              108
#define C_INPUT_TYPE                109
#define C_CHAR_LIST                 110
#define C_OBSCURED_INPUT            111
#define C_MAX_LENGTH                112
#define C_ORIGINAL_LABEL            113
#define C_BUTTON_STYLE              114
#define C_ACTIVATE                  115
#define C_ADD                       116
#define C_ADD_ITEM                  117
#define C_APPEND                    118
#define C_BRING_TO_FRONT            119
#define C_CALL                      120
#define C_CALL_ACTION_SLOT          121
#define C_CLEAR                     122
#define C_CLONE                     123
#define C_CLOSE_CONNECTION          124
#define C_DEACTIVATE                125
#define C_DEL_ITEM                  126
#define C_DESELECT                  127
#define C_DESELECT_ITEM             128
#define C_DIVIDE                    129
#define C_DRAW_ARC                  130
#define C_DRAW_LINE                 131
#define C_DRAW_OVAL                 132
#define C_DRAW_POLYGON              133
#define C_DRAW_POLYLINE             134
#define C_DRAW_RECTANGLE            135
#define C_DRAW_SECTOR               136
#define C_FORK                      137
#define C_GET_AVAILABILITY_STATUS   138
#define C_GET_BOX_SIZE              139
#define C_GET_CELL_ITEM             140
#define C_GET_CURSOR_POSITION       141
#define C_GET_ENGINE_SUPPORT        142
#define C_GET_ENTRY_POINT           143
#define C_GET_FILL_COLOUR           144
#define C_GET_FIRST_ITEM            145
#define C_GET_HIGHLIGHT_STATUS      146
#define C_GET_INTERACTION_STATUS    147
#define C_GET_ITEM_STATUS           148
#define C_GET_LABEL                 149
#define C_GET_LAST_ANCHOR_FIRED     150
#define C_GET_LINE_COLOUR           151
#define C_GET_LINE_STYLE            152
#define C_GET_LINE_WIDTH            153
#define C_GET_LIST_ITEM             154
#define C_GET_LIST_SIZE             155
#define C_GET_OVERWRITE_MODE        156
#define C_GET_PORTION               157
#define C_GET_POSITION              158
#define C_GET_RUNNING_STATUS        159
#define C_GET_SELECTION_STATUS      160
#define C_GET_SLIDER_VALUE          161
#define C_GET_TEXT_CONTENT          162
#define C_GET_TEXT_DATA             163
#define C_GET_TOKEN_POSITION        164
#define C_GET_VOLUME                165
#define C_LAUNCH                    166
#define C_LOCK_SCREEN               167
#define C_MODULO                    168
#define C_MOVE                      169
#define C_MOVE_TO                   170
#define C_MULTIPLY                  171
#define C_OPEN_CONNECTION           172
#define C_PRELOAD                   173
#define C_PUT_BEFORE                174
#define C_PUT_BEHIND                175
#define C_QUIT                      176
#define C_READ_PERSISTENT           177
#define C_RUN                       178
#define C_SCALE_BITMAP              179
#define C_SCALE_VIDEO               180
#define C_SCROLL_ITEMS              181
#define C_SELECT                    182
#define C_SELECT_ITEM               183
#define C_SEND_EVENT                184
#define C_SEND_TO_BACK              185
#define C_SET_BOX_SIZE              186
#define C_SET_CACHE_PRIORITY        187
#define C_SET_COUNTER_END_POSITION  188
#define C_SET_COUNTER_POSITION      189
#define C_SET_COUNTER_TRIGGER       190
#define C_SET_CURSOR_POSITION       191
#define C_SET_CURSOR_SHAPE          192
#define C_SET_DATA                  193
#define C_SET_ENTRY_POINT           194
#define C_SET_FILL_COLOUR           195
#define C_SET_FIRST_ITEM            196
#define C_SET_FONT_REF              197
#define C_SET_HIGHLIGHT_STATUS      198
#define C_SET_INTERACTION_STATUS    199
#define C_SET_LABEL                 200
#define C_SET_LINE_COLOUR           201
#define C_SET_LINE_STYLE            202
#define C_SET_LINE_WIDTH            203
#define C_SET_OVERWRITE_MODE        204
#define C_SET_PALETTE_REF           205
#define C_SET_PORTION               206
#define C_SET_POSITION              207
#define C_SET_SLIDER_VALUE          208
#define C_SET_SPEED                 209
#define C_SET_TIMER                 210
#define C_SET_TRANSPARENCY          211
#define C_SET_VARIABLE              212
#define C_SET_VOLUME                213
#define C_SPAWN                     214
#define C_STEP                      215
#define C_STOP                      216
#define C_STORE_PERSISTENT          217
#define C_SUBTRACT                  218
#define C_TEST_VARIABLE             219
#define C_TOGGLE                    220
#define C_TOGGLE_ITEM               221
#define C_TRANSITION_TO             222
#define C_UNLOAD                    223
#define C_UNLOCK_SCREEN             224
#define C_NEW_GENERIC_BOOLEAN       225
#define C_NEW_GENERIC_INTEGER       226
#define C_NEW_GENERIC_OCTETSTRING   227
#define C_NEW_GENERIC_OBJECT_REF    228
#define C_NEW_GENERIC_CONTENT_REF   229
#define C_NEW_COLOUR_INDEX          230
#define C_NEW_ABSOLUTE_COLOUR       231
#define C_NEW_FONT_NAME             232
#define C_NEW_FONT_REFERENCE        233
#define C_NEW_CONTENT_SIZE          234
#define C_NEW_CONTENT_CACHE_PRIO    235
#define C_INDIRECTREFERENCE         236
/* UK MHEG */
#define C_SET_BACKGROUND_COLOUR     237
#define C_SET_CELL_POSITION         238
#define C_SET_INPUT_REGISTER        239
#define C_SET_TEXT_COLOUR           240
#define C_SET_FONT_ATTRIBUTES       241
#define C_SET_VIDEO_DECODE_OFFSET   242
#define C_GET_VIDEO_DECODE_OFFSET   243
#define C_GET_FOCUS_POSITION        244
#define C_SET_FOCUS_POSITION        245
#define C_SET_BITMAP_DECODE_OFFSET  246
#define C_GET_BITMAP_DECODE_OFFSET  247
#define C_SET_SLIDER_PARAMETERS     248
// Added in ETSI ES 202 184 V2.1.1 (2010-01)
#define C_GET_DESKTOP_COLOUR        249
#define C_SET_DESKTOP_COLOUR        250
#define C_GET_COUNTER_POSITION      251
#define C_GET_COUNTER_MAX_POSITION  252

// Pseudo-codes.  These are encoded into the link condition in binary but it's convenient
// to give them codes here since that way we can include them in the same lookup table.
#define P_EVENT_SOURCE              249
#define P_EVENT_TYPE                250
#define P_EVENT_DATA                251
// The :ActionSlots tag appears in the textual form but not in binary.
#define P_ACTION_SLOTS              252


#endif
