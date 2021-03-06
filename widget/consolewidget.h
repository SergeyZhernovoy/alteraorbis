#ifndef ALTERA_CONSOLE_WIDGET
#define ALTERA_CONSOLE_WIDGET

#include "../gamui/gamui.h"
#include "../grinliz/glstringutil.h"
#include "../grinliz/glcontainer.h"
#include "../grinliz/glvector.h"

class ConsoleWidget : public gamui::IWidget
{
public:
	ConsoleWidget();
	void Init( gamui::Gamui* );
	void SetBackground(gamui::RenderAtom atom);

	virtual float X() const							{ return lines[0].text.X(); }
	virtual float Y() const							{ return lines[0].text.Y(); }
	virtual float Width() const						{ return lines[0].text.Width(); }
	virtual float Height() const;
	virtual void SetPos(float x, float y);

	virtual void SetSize(float w, float h);
	virtual bool Visible() const					{ return lines[0].text.Visible(); }
	virtual void SetVisible(bool vis);

	void Push( const grinliz::GLString &str );
	void Push( const grinliz::GLString &str, const gamui::RenderAtom& icon, const grinliz::Vector2F& pos, const gamui::RenderAtom& background );
	void DoTick( U32 delta );
	bool IsItem(const gamui::UIItem* item, grinliz::Vector2F* pos);

	void SetTime(int time) { ageTime = time; }

private:
	void Scroll();

	struct Line {
		Line() { age = 0; pos.Zero(); }
		void SetHighLightSize();

		gamui::TextLabel	text;
		int					age;
		gamui::PushButton	button;
		grinliz::Vector2F	pos;
		gamui::Image		highLight;
	};

	enum { 
		NUM_LINES = 10,			// expediant hack
		AGE_TIME  = 40*1000,	// msec
	};
	gamui::Gamui*		gamui;
	gamui::Image		background;
	int					ageTime;
	int					nLines;
	Line				lines[NUM_LINES];
};


#endif // ALTERA_CONSOLE_WIDGET


