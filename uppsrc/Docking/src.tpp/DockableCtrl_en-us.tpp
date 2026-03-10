topic "DockableCtrl";
[i448;a25;kKO9;*@(64)2 $$1,0#37138531426314131252341829483380:class]
[l288;2 $$2,2#27521748481378242620020725143825:desc]
[a83;*R6 $$3,0#31310162474203024125188417583966:caption]
[l288;i1121;b17;O9;~~~.1408;2 $$4,0#10431211400427159095818037425705:param]
[i448;a25;kKO9;*@(64)2 $$5,0#37138531426314131252341829483370:item]
[*+117 $$6,6#14700283458701402223321329925657:header]
[l416;2 $$7,7#55548704457842300043401641954952:nested`-desc]
[l288;i448;a25;kO9;*2 $$8,8#64691275497409617375831514634295:nested`-class]
[H6;0 $$9,0#05600065144404261032431302351956:begin]
[0 $$10,0#96390100711032703541132217272105:end]
[2 $$0,0#00000000000000000000000000000000:Default]
[{_}%EN-US 
[s3; DockableCtrl&]
[s2;l0;i64; The DockableCtrl class provide a common container and 
interface for Ctrl`'s that wish to be managed by the Docking 
framework. It is deliberately light`-weight but provides methods 
for:&]
[s2;l0;i150;O3; Setting size`-hints that allow the DockWindow to 
better size and place the control with the dock frames&]
[s2;l0;i150;O3; Property methods for setting an icon, title and window 
group&]
[s2;l0;i150;O3; A Chameleon style interface enabling a custom appearance 
of the docking handle (the frame that displays the title and 
icon)&]
[s2;l0;i150;O3; Status methods for determining it`'s current position 
in the framework/layout.&]
[s0;*@(0.0.255) &]
[s1;%- [@(0.0.255) class]_[@0 DockableCtrl]_:_[@(0.0.255) public]_[^`:`:Bar^ ParentCtrl]&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:WhenMenuBar:%- [^`:`:Callback1^ Callback1]<Bar_`&>_[@0 WhenMenu
Bar]&]
[s2;   Executed when the dockable ctrl`'s window menu is shown. This 
when the menu button is pressed or a context menu is activated 
on the handle/title bar of the window.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:WindowMenu`(`:`:Bar`&`):%- virtual [@(0.0.255) void]_[@0 Window
Menu]([^`:`:Bar^ Bar]_`&[@3 bar])&]
[s2;   Override called when the window menu is shown. Default behaviour 
is to execute WhenMenuBar.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:GetIcon`(`):%- [@(0.0.255) const]_[^`:`:Image^ Image]_`&_[@0 GetI
con]()&]
[s2;   Returns the image assigned to the ctrl.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:`:`:Icon`(const`:`:Image`&`):%- [^`:`:DockableCtrl^ DockableC
trl]_`&_[@0 Icon]([@(0.0.255) const]_[^`:`:Image^ Image]`&_[@3 m])&]
[s2;   Sets the image assigned to the ctrl.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:Title`(const char`*`):%- [^`:`:DockableCtrl^ DockableCtrl]_`&
_[@0 Title]([@(0.0.255) const]_[@(0.0.255) char]_`*[@3 `_title])&]
[s2;   Sets the ctrl`'s title.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:Title`(const`:`:WString`&`):%- [^`:`:DockableCtrl^ DockableCt
rl]_`&_[@0 Title]([@(0.0.255) const]_[^`:`:WString^ WString]`&_[@3 `_title])&]
[s2;   Sets the ctrl`'s title.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:GetTitle`(`):%- [@(0.0.255) const]_[^`:`:WString^ WString]_`&_[@0 G
etTitle]()&]
[s2;   Returns the current title.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:SizeHint`(const`:`:Size`&`,const`:`:Size`&`,const`:`:Size`&`):%- [^`:`:DockableCtrl^ D
ockableCtrl]_`&_[@0 SizeHint]([@(0.0.255) const]_[^`:`:Size^ Size]_`&[@3 min], 
[@(0.0.255) const]_[^`:`:Size^ Size]_`&[@3 max]_`=_Null, [@(0.0.255) const]_[^`:`:Size^ Siz
e]_`&[@3 std]_`=_Null)&]
[s2;   Shortcut to setting minimum, standard and maximum size to 
[%-*@3 min] [%-*@3 max] and [%-*@3 std] respectively. If Null is passed 
the hint remains unchanged.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:SetMinSize`(`:`:Size`):%- [@(0.0.255) void]_[@0 SetMinSize]([^`:`:Size^ S
ize]_[@3 sz])&]
[s2;%- [%%   ]Sets the minimum size for the ctrl to [*@3 sz].&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:SetMaxSize`(`:`:Size`):%- [@(0.0.255) void]_[@0 SetMaxSize]([^`:`:Size^ S
ize]_[@3 sz])&]
[s2;   Sets the maximum size of the ctrl to [%-*@3 sz].&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:SetStdSize`(`:`:Size`):%- [@(0.0.255) void]_[@0 SetStdSize]([^`:`:Size^ S
ize]_[@3 sz])&]
[s2;   Sets the best/standard size of the ctrl to [%-*@3 sz] &]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:GetMinSize`(`)const:%- virtual [^`:`:Size^ Size]_[@0 GetMinSize
]()_[@(0.0.255) const]&]
[s2;   Returns the ctrl`'s minimum size.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:GetMaxSize`(`)const:%- virtual [^`:`:Size^ Size]_[@0 GetMaxSize
]()_[@(0.0.255) const]&]
[s2;   Returns the ctrl`'s maximum size.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:GetStdSize`(`)const:%- virtual [^`:`:Size^ Size]_[@0 GetStdSize
]()_[@(0.0.255) const]&]
[s2;   Returns the ctrl`'s best/standard size.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:SetStyle`(const`:`:DockableCtrl`:`:Style`&`):%- [^`:`:DockableCtrl^ D
ockableCtrl]_`&_[@0 SetStyle]([@(0.0.255) const]_[^`:`:DockableCtrl`:`:Style^ Style]_`&
[@3 s])&]
[s2;   Sets the current Chameleon style to [%-*@3 s]. [@3 s] should not 
be allowed to run out of scope or otherwise be destroyed by the 
calling function.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:GetStyle`(`):%- [@(0.0.255) const]_[^`:`:DockableCtrl`:`:Style^ S
tyle]_`&_[@0 GetStyle]()&]
[s2;   Returns th current Chameleon style.&]
[s0; &]
[s5;i288;K:`:`:DockableCtrl`:`:StyleDefault`(`):%- static [@(0.0.255) const]_[^`:`:DockableCtrl`:`:Style^ S
tyle]`&_[@0 StyleDefault]()&]
[s2;l0;i288;   Returns the default Chameleon style.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:StyleDefaultVert`(`):%- static [@(0.0.255) const]_[^`:`:DockableCtrl`:`:Style^ S
tyle]`&_[@0 StyleDefaultVert]()&]
[s2;   Returns the default Chameleon style for vertical handle.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:GetGroup`(`)const:%- virtual [@(0.0.255) const]_[^`:`:String^ S
tring]_`&[@0 GetGroup]()_[@(0.0.255) const]&]
[s2;   Returns the current group the ctrl is a member of, or Null 
if it is not in a group.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:SetGroup`(const`:`:String`&`):%- virtual 
[^`:`:DockableCtrl^ DockableCtrl]_`&[@0 SetGroup]([@(0.0.255) const]_[^`:`:String^ String
]_`&[@3 g])&]
[s2;%- [%%   Sets the ctrls group to ][*@3 g].&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:AllowDockAll`(`):%- [^`:`:DockableCtrl^ DockableCtrl]_`&_[@0 Al
lowDockAll]()&]
[s2;   Allows docking for all alignments. Both the ctrl and the DockWindow 
managing it must allow docking.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:AllowDockNone`(`):%- [^`:`:DockableCtrl^ DockableCtrl]_`&_[@0 A
llowDockNone]()&]
[s2;   Denys docking for all alignments. Both the ctrl and the DockWindow 
managing it must allow docking.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:AllowDockLeft`(bool`):%- [^`:`:DockableCtrl^ DockableCtrl]_`&
_[@0 AllowDockLeft]([@(0.0.255) bool]_[@3 v]_`=_true)&]
[s2;  Allows/Denys docking permission for the [* left] frame. Both 
the ctrl and the DockWindow managing it must allow docking.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:AllowDockTop`(bool`):%- [^`:`:DockableCtrl^ DockableCtrl]_`&_
[@0 AllowDockTop]([@(0.0.255) bool]_[@3 v]_`=_true)&]
[s2;   Allows/Denys docking permission for the [* top] frame. Both 
the ctrl and the DockWindow managing it must allow docking.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:AllowDockRight`(bool`):%- [^`:`:DockableCtrl^ DockableCtrl]_`&
_[@0 AllowDockRight]([@(0.0.255) bool]_[@3 v]_`=_true)&]
[s2;   Allows/Denys docking permission for the [* right ]frame. Both 
the ctrl and the DockWindow managing it must allow docking.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:AllowDockBottom`(bool`):%- [^`:`:DockableCtrl^ DockableCtrl]_
`&_[@0 AllowDockBottom]([@(0.0.255) bool]_[@3 v]_`=_true)&]
[s2;   Allows/Denys docking permission for the [* bottom ]frame. Both 
the ctrl and the DockWindow managing it must allow docking.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:AllowDock`(int`,bool`):%- [^`:`:DockableCtrl^ DockableCtrl]_`&
_[@0 AllowDock]([@(0.0.255) int]_[@3 a], [@(0.0.255) bool]_[@3 v]_`=_true)&]
[s2;   Allows/Denys docking permission for the frame indicated by 
alignment [*@3 a]. Both the ctrl and the DockWindow managing it 
must allow docking.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:IsDockAllowed`(int`):%- [@(0.0.255) bool]_[@0 IsDockAllowed]([@(0.0.255) i
nt]_[@3 a])&]
[s2;   Returns true if docking is allowed for alignment [%-*@3 a]. 
Does not account for permissions set on the managing DockWindow.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:IsFloating`(`)const:%- [@(0.0.255) bool]_[@0 IsFloating]()_[@(0.0.255) c
onst]&]
[s2;   Returns true if the ctrl is currently floating (open as a 
normal child window).&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:IsDocked`(`)const:%- [@(0.0.255) bool]_[@0 IsDocked]()_[@(0.0.255) c
onst]&]
[s2;   Returns true if the window is docked at any alignment.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:IsAutoHide`(`)const:%- [@(0.0.255) bool]_[@0 IsAutoHide]()_[@(0.0.255) c
onst]&]
[s2;   Return true if the ctrl is currently being auto`-hidden.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:IsTabbed`(`)const:%- [@(0.0.255) bool]_[@0 IsTabbed]()_[@(0.0.255) c
onst]&]
[s2;   Returns true if the ctrl is currently displayed as a tab.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:IsHidden`(`)const:%- [@(0.0.255) bool]_[@0 IsHidden]()_[@(0.0.255) c
onst]&]
[s2;   Returns true if the ctrl is current hidden/closed.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:GetDockAlign`(`)const:%- [@(0.0.255) int]_[@0 GetDockAlign]()_[@(0.0.255) c
onst]&]
[s2;   Returns the alignment if the ctrl is docked.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:Highlight`(`):%- [@(0.0.255) void]_[@0 Highlight]()&]
[s2;   Displays a visual highlight on the ctrl to indicate it`'s 
position to the user.&]
[s0; &]
[s5;K:`:`:DockableCtrl`:`:`:`:DockableCtrl`(`):%- [@0 DockableCtrl]()&]
[s2;   Class Contructor.&]
[s0; &]
[s9;%- &]
[s5;:Upp`:`:DockableCtrl`:`:ShowTab`(`)const:%- [@(0.0.255) void][@0  
][@0 ShowTab][@0 () ][@(0.0.255) const]&]
[s2; Bring tab window to foreground&]
[s10;%- &]
[s0; ]]