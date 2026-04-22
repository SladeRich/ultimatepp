#include "CtrlLib.h"

#ifdef GUI_X11

namespace Upp {

void ChHostSkin()
{
	String s = Sys("dump_xsettings");
	StringStream ss(s);
	String font_name, theme;
	int    scaling = 1;
	int    xdpi = 98347;
	while(!ss.IsEof()) {
		String l = ss.GetLine();
		int q = l.Find(' ');
		if(q >= 0) {
			String id = l.Mid(0, q);
			String value = l.Mid(q + 1);
			if(id == "Gdk/WindowScalingFactor")
				scaling = max(Atoi(value), 1);
			if(id == "Gtk/FontName")
				font_name = value;
			if(id == "Xft/DPI")
				xdpi = Nvl(StrInt(value), 98347);
			if(id == "Net/ThemeName")
				theme = value;
		}
	}

	int fontface = Font::ARIAL;
	int fontheight = 13;
	bool bold = false;
	bool italic = false;

	const char *q = strrchr(font_name, ' ');
	if(q) {
		int h = atoi(q);
		if(h)
			fontheight = h;
		String face(font_name, q);
		fontface = Font::FindFaceNameIndex(face);

		if(fontface == 0) {
			for(;;) {
				const char *q = strrchr(face, ' ');
				if(!q) break;
				const char *s = q + 1;
				if(stricmp(s, "Bold") == 0 || stricmp(s, "Heavy") == 0)
					bold = true;
				else
				if(stricmp(s, "Italic") == 0 || stricmp(s, "Oblique") == 0)
					italic = true;
				else
				if(stricmp(s, "Regular") == 0 || stricmp(s, "Light") || stricmp(s, "Medium"))
					;
				else
					continue;
				face = String(~face, q);
			}
			fontface = Font::FindFaceNameIndex(face);
			if(fontface == 0) {
				if(ToUpper(face[0]) == 'M')
					fontface = Font::COURIER;
				else
				if(ToUpper(face[0]) == 'S' && ToUpper(face[1]) == 'e')
					fontface = Font::ROMAN;
				else
					fontface = Font::ARIAL;
			}
		}
	}
	
	Font gui_font = Font(fontface, fround(fontheight * xdpi / (72*1024.0))).Bold(bold).Italic(italic);
	Font::SetDefaultFont(gui_font);

	SColorFace_Write(Color(242, 241, 240));
	SColorMenu_Write(Color(242, 241, 240));
	SColorHighlight_Write(Color(50, 50, 250));

	auto ThemeHasWord = [&](const char *text) {
		int q = ToLower(theme).Find(text);
		if(q >= 0) {
			if(q > 0) {
				int pc = theme[q - 1];
				if(!(IsUpper(theme[q]) && IsLower(pc) || !IsLetter(pc)))
					return false;
			}
			int l = strlen(text);
			int nc = theme[q + l];
			if(!(IsLower(text[l - 1]) && IsUpper(nc) || !IsLetter(nc)))
				return false;
			return true;
		}
		return false;
	};
	
	if(ThemeHasWord("dark") || ThemeHasWord("inverse") || ThemeHasWord("black"))
		ChDarkSkin();
	else
		ChStdSkin();
}

}

#endif
