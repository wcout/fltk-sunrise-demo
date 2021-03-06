/*
 FLTK Sunrise demo.

 (c) 2017 wcout wcout<gmx.net>

 "A non-scientific but rather artistic astronomical demo
 of sunrise, stars, milky way and sparks - coded in the
 principle of mimimalism and stopped enhancing before it
 got messy...".

 This code is free software: you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by
 the Free Software Foundation,  either version 3 of the License, or
 (at your option) any later version.

 This code is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY;  without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 See the GNU General Public License for more details:
 http://www.gnu.org/licenses/.

*/
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>
#include <cmath>
#include <ctime>
#include <vector>
#include <string>
#include <cassert>

using namespace std;

static const int HaloSize = 10; 	// in sun radius unit
static int FPS = 20;

static uchar *make_alpha_box( Fl_Color c_, int w_, int h_, int alpha_ )
{
	uchar *image = new uchar[4 * w_ * h_];
	uchar *p = image;
	uchar r, g, b;
	Fl::get_color( c_, r, g, b );
	for ( int y = 0; y < h_; y++ )
	{
		for ( int x = 0; x < w_; x++ )
		{
			*p++ = r;
			*p++ = g;
			*p++ = b;
			if ( alpha_ < 255 )
				*p++ = uchar( alpha_ );	// alpha transparency
			else *p++ = 255;
		}
	}
	return image;
}

class Nebula
{
public:
	Nebula( int x_, int y_, int w_, int h_, uchar t_ ) :
		x( x_ ), y( y_ ), w( w_ ), h( h_ ), t( t_ ), img( 0 )
	{
		uchar *data = make_alpha_box( FL_WHITE, w, h, t );
		img = new Fl_RGB_Image( data, w, h, 4 );
		img->alloc_array = 1;
	}
	~Nebula()
	{
		delete img;
	}
	void draw()
	{
		draw( x, y );
	}
	void draw( int x_, int y_ )
	{
		if ( img )
			img->draw( x_, y_ );
	}
	int x, y, w, h;
	uchar t;
private:
	Nebula( const Nebula& ) {} // forbid copy()
	Fl_RGB_Image *img;
};

class Cloud
{
public:
	Cloud( int x_, int y_, int w_, int h_ ) :
		x( x_ ), y( y_ ), w( w_ ), h( h_ ),
		speed( 1. ),
		flockDissolve( 0. )
	{
		speed = 0.5 * (double)( random() % 3 + 1 );
		int flocks = x * w / 400;
		int W = ceil( (double)w / 10 );
		int H = ceil( (double)h / 10 );
		for ( int i = 0; i < flocks; i++ )
		{
			while ( 1 )
			{
				int x = random() % w;
				int y = random() % h;
				if ( x < w / 5 && y < h / 5 ) continue;
				if ( x > w - w / 5 && y > h - h / 5 ) continue;
				if ( x < w / 5 && y > h - h / 5 ) continue;
				if ( x > w - w / 5 && y < h / 5 ) continue;
				Nebula *flock = new Nebula( x, y, random() % W + 5, random() % H + 5, random() % 30 + 5 );
				_flocks.push_back( flock );
				break;
			}
		}
	}
	~Cloud()
	{
		for ( size_t i = 0; i < _flocks.size(); i++ )
			delete _flocks[i];
	}
	void draw()
	{
		draw( x, y );
	}
	void draw( int x_, int y_ )
	{
		if ( flockDissolve >= 0. && flockDissolve < 1. )
		{
			// draw only flocks with alpha above "dissolve level",
			// this will make the (dis-)appearing process more naturally
			// (with simple means)
			uchar t = flockDissolve * 256;
			for ( size_t i = 0; i < _flocks.size(); i++ )
				if ( _flocks[i]->t >= t )
					_flocks[i]->draw( x_ + _flocks[i]->x, y_ + _flocks[i]->y );
		}
	}
	int x, y, w, h;
	double speed;
	double flockDissolve;
private:
	vector<Nebula *> _flocks;
};

class Star
{
public:
	Star( int x_, int y_, int d_, Fl_Color color_ = FL_WHITE ) :
		x( x_ ), y( y_ ), d( d_ ), color( color_ ), data( 0 )
	{
	}
	int x, y, d;
	Fl_Color color;
	int data;
};

class Sunrise : public Fl_Double_Window
{
typedef Fl_Double_Window Inherited;
public:
	Sunrise() : Inherited( 1024, 768, "FLTK sunrise demo" ),
		_debug( false ),
		_sun_angle( 170. ),	// start shortly before sunrise
		_moon_angle( 10 ),	// start shortly after moonset
		_bg( fl_rgb_color( fl_darker( FL_DARK_BLUE ) ) ),
		_sun_r( 0 ),
		_frame( 0 ),
		_to( 1. / FPS ),
		_hold( false ),
		_no_moon( false ),
		_cloud_dissolve_f( 1. )
	{
		moveSun();	// initialize remaining values
		moveMoon();	// initialize remaining values
		color( FL_BLACK );
		resizable( this );
		end();
		show();
		wait_for_expose();
	}
	void drawBg()
	{
		fl_rectf( 0, 0, w(), h(), _bg );
	}
	void drawClouds()
	{
		for ( size_t i = 0; i < _clouds.size(); i++ )
		{
			int c_x = (int)( _clouds[i]->x + _clouds[i]->speed * (double)_frame / 2. ) % w();
			int c_y = _clouds[i]->y;
			_clouds[i]->flockDissolve = _cloud_dissolve_f;
			_clouds[i]->draw( c_x, c_y );
		}
	}
	void drawNebula()
	{
		int step = std::max( 1, (int)( 10 + ( _zenith * 10 ) ) );
		for ( size_t i = 0; i < _nebula.size(); i += step )
		{
			int n_x = (int)( _nebula[i]->x + (double)_frame / 3. ) % w();
			int n_y = _nebula[i]->y;
			_nebula[i]->draw( n_x, n_y );
		}
	}
	void drawSparks()
	{
		if ( random() % 100 == 0 )
		{
			int x0 = random() % ( w() / 2 ) + w() / 3;
			int y0 = random() % ( h() / 8 );
			int x1 = random() % ( w() / 2 ) + w() / 3;
			int y1 = random() % ( h() / 8 ) + h() / 2;
			fl_color( FL_WHITE );
			fl_line( x0, y0, x1, y1 );
		}
	}
	void drawStars()
	{
		for ( size_t i = 0; i < _stars.size(); i++ )
		{
			int star_x = (int)( _stars[i].x + (double)_frame / 3. ) % w();
			int star_y = _stars[i].y;
			int sun_x = _sun_x;
			int sun_y = h() - _sun_y + _sun_r;
			int sun_dist = (int)sqrt( abs( star_x - sun_x ) * abs( star_x - sun_x ) +
			                          abs( star_y - sun_y ) * abs( star_y - sun_y ) );

			// let no stars be visible beneath sun
			if ( ( ( _sun_angle > 160. && _sun_angle < 360. ) || _sun_angle < 20. ) &&
			     sun_dist < 6 * _sun_r )
				continue;

			Fl_Color color = fl_color_average( _bg, _stars[i].color, std::min( 1., nightFactor() * 2 ) );
			int d = _stars[i].d;

			if ( w() >= 400 && h() >= 400 )
			{
				int f = ceil( (double)d / 2 );
				if ( f && !rising() )
					d += random() % f - f / 2;

				// draw star lens reflection
				if ( _sun_angle > 20. && _sun_angle < 160. &&
				     d > (double)_sun_r / 8 && _stars[i].data++ % 10 > 5 )
				{
					fl_color( fl_darker( color ) );
					int cx = star_x + d / 2;
					int cy = star_y + d / 2;
					int l = _sun_r / 4;
					fl_line( cx - l, cy - l, cx + l, cy + l + 1 );
					fl_line( cx - l, cy + l, cx + l, cy - l - 1 );
				}
			}
			fl_color( color );
			if ( d <= 1 )
				fl_point( star_x, star_y );
			else
				fl_pie( star_x, star_y, d, d, 0., 360. );
		}
	}
	void drawHalo()
	{
		Fl_Color sun_color = fl_color_average( fl_lighter( FL_YELLOW ), FL_DARK_RED, zenith() );
		for ( double i = HaloSize; i > 1; i -= 0.1 )
		{
			fl_color( fl_color_average( sun_color, _bg, 1. / (double)(i) ) );
			fl_pie( _sun_x - i * (double)_sun_r, h() - _sun_r -_sun_y - (double)_sun_r * (i - 1),
                 _sun_r * 2 * i, _sun_r * 2 * i, 0., 360. );
		}
	}
	void drawMoon()
	{
		double angle_diff = _moon_angle - _sun_angle;
		if ( angle_diff < 0. )
			angle_diff += 360;
		double phase = 2 * ( angle_diff / 360. - 0.5 );
		Fl_Color moon_color = _zenith < 0. ?
			fl_color_average( FL_WHITE, FL_YELLOW, fabs( _moon_zenith ) ) :
			fl_color_average( FL_WHITE, FL_GRAY, fabs( _moon_zenith ) );
		fl_color( moon_color );
		fl_pie( _moon_x - _sun_r, h() - _moon_y - _sun_r, _sun_r * 2, _sun_r * 2, 0., 360. );
		fl_color( fl_darker( _bg ) );
		if ( phase > 0. )
		{
			fl_pie( _moon_x - _sun_r * phase, h() - _moon_y - _sun_r, _sun_r * 2 * phase, _sun_r * 2, 0., 360. );
			fl_pie( _moon_x - _sun_r, h() - _moon_y - _sun_r, _sun_r * 2, _sun_r * 2, 90., 270. );
		}
		else
		{
			fl_pie( _moon_x - _sun_r * fabs( phase ), h() - _moon_y - _sun_r, _sun_r * 2 * fabs( phase ), _sun_r * 2, 0., 360. );
			fl_pie( _moon_x - _sun_r, h() - _moon_y - _sun_r, _sun_r * 2, _sun_r * 2, 270., 450. );
		}
	}
	void drawSun()
	{
		Fl_Color sun_color = fl_color_average( fl_lighter( FL_YELLOW ), FL_DARK_RED, zenith() );
		fl_color( sun_color );
		fl_pie( _sun_x - _sun_r, h() - _sun_y - _sun_r, _sun_r * 2, _sun_r * 2, 0., 360. );
	}
	void toggle_fullscreen()
	{
		if ( fullscreen_active() )
		{
			fullscreen_off();
			cursor( FL_CURSOR_DEFAULT );
		}
		else
		{
			fullscreen();
			cursor( FL_CURSOR_NONE );
		}
	}
	void init()
	{
		_sun_r = w() / 30;
		// correction for non 4:3 window sizes (which this demo is designed for)
		double f = ( (double)h() / w() ) / 0.75;
		_sun_r = f * _sun_r;

		int d = ceil( (double)_sun_r / 9 );
		_stars.clear();
		for ( int i = 0; i < 200; i++ )
		{
			int color = FL_WHITE;
			if ( i % 90 == 0 )
				color = fl_lighter( FL_RED );
			else if ( i % 20 == 0 )
				color = FL_YELLOW;
			_stars.push_back( Star( random() % w(), random() % h(),
			                  random() % d * random() % d + 1 + 2 * !(random() % 40),
			                  color ) );
		}

		for ( size_t i = 0; i < _clouds.size(); i++ )
			delete _clouds[i];
		_clouds.clear();
		int H = h() / 3;
		for ( int i = 0; i < random() % 10 + 5; i++ )
		{
			int x = random() % w();
			int y = random() % H + ( h() - H ) / 3;
			int W = random() % ( w() / 5 ) + 4 * _sun_r;
			int H = W / 2;
			_clouds.push_back( new Cloud( x, y, W, H ) );
		}

		for ( size_t i = 0; i < _nebula.size(); i++ )
			delete _nebula[i];
		_nebula.clear();
		/*int*/ H = h() / 3;
		for ( int i = 0; i < 1000; i++ )
		{
			if ( i % 100 == 0 )
				H = h() / ( ( random() % 3 ) + 2 );
			int x = random() % w();
			int y = random() % H + ( h() - H ) / 3;
			int W = random() % w() / 100 + 10;
			int H = random() % h() / 100 + 10;
			double t = random() % 30 + 5;
			_nebula.push_back( new Nebula( x, y, W, H, t ) );
		}
	}
	int handle( int e_ )
	{
		int ret = Inherited::handle( e_ );
		if ( e_ == FL_KEYDOWN )
		{
			int c = Fl::event_key();
			if ( c == ' ' )
			{
				_hold = !_hold;
				onTimer();
			}
			else if ( c == 'f' )
			{
				toggle_fullscreen();
			}
			else if ( c == 's' || c == '+' || c == '-' )
			{
				FPS +=  c == '-' ? -10 : 10;
				if ( FPS > 100 )
					FPS = 20;
				if ( FPS < 20 )
					FPS = 100;
				_to = 1. / FPS;
			}
			else if ( c == 'd' )
			{
				_debug = !_debug;
			}
			else if ( c == 'm' )
			{
				_no_moon = !_no_moon;
				if ( !_no_moon )
				{
					_moon_angle = _sun_angle - random() % 30;
					if ( _moon_angle < 0. )
						_moon_angle += 360.;
				}
			}
		}
		return ret;
	}
	void drawInfo()
	{
		char buf[50];
		double angle( _sun_angle + 180. );
		if ( angle > 360. )
			angle -= 360;
		snprintf( buf, sizeof( buf ), "[%dfps] %3.1f%% (%3.1f°)",
			FPS, _zenith * 100, angle );
		fl_font( FL_HELVETICA, _sun_r  );
		fl_color( FL_BLUE );
		fl_draw( buf, w() / 2 - _sun_r * 4, h() - _sun_r );
	}
	void draw()
	{
		static const double cloud_max = 0.9;
		static const double cloud_min = 0.35;
		drawBg();
		drawHalo();
		if ( _zenith < -0.5 )
			drawNebula();
		drawStars();
		if ( _zenith < -0.4 )
			drawSparks();
		drawSun();
		if ( !_no_moon )
			drawMoon();
		if ( _zenith > cloud_min )
		{
			// let clouds appear not instantaneaously but slowly by
			// calculating a visibility factor from the zenith value
			_cloud_dissolve_f = _zenith < cloud_max ?
				( 1. / ( cloud_max - cloud_min ) ) * ( cloud_max - _zenith ) : 0.;
			_cloud_dissolve_f *= _cloud_dissolve_f;
			_cloud_dissolve_f *= _cloud_dissolve_f;
			drawClouds();
		}
		if ( _debug )
			drawInfo();
	}
	void moveSunOrMoon( double angle_inc_, double& angle_, int &x_, int& y_, double& zenith_ )
	{
		angle_ += angle_inc_;
		if ( angle_ > 360. )
			angle_ = 0.;

		// fit elliptical path within view so that sun/moon disc is
		// always completely within view when above horizon
		int H = ( h() - _sun_r ) * 2;
		int W = w() - 2 * _sun_r;
		int cx = W / 2;
		int cy = H / 2;
		x_ = cx + cos( angle_ * M_PI / 180. ) * cx;
		y_ = cy + sin( angle_ * M_PI / 180. ) * cy;
		y_ -= H / 2;
		y_ *= -1;
		x_ += _sun_r;

		zenith_ = (double)y_ / ( H / 2 ); // [-1, +1]
	}
	void moveSun()
	{
		static const double angle_inc = 0.05;
		moveSunOrMoon( angle_inc, _sun_angle, _sun_x, _sun_y, _zenith );
	}
	void moveMoon()
	{
		static const double angle_inc = 0.05 + 0.004;
		moveSunOrMoon( angle_inc, _moon_angle, _moon_x, _moon_y, _moon_zenith );
	}

	void onTimer()
	{
		if ( _hold )
			return;
		_bg = fl_color_average( FL_CYAN, fl_darker( FL_DARK_BLUE ), nightFactor() );
		_frame++;
		moveSun();
		moveMoon();
		Fl::repeat_timeout( _to, cb_timer, this );
		redraw();
	}
	static void cb_timer( void *d_ )
	{
		(static_cast<Sunrise *>( d_ ))->onTimer();
	}
	virtual void resize( int x_, int y_, int w_, int h_ )
	{
		int W = w();
		int H = h();
		Inherited::resize( x_, y_, w_, h_ );
		if ( w_ != W || h_ != H )
		{
			_sun_x = (double)_sun_x * (double)w() / W;
			init();
		}
	}
	void run( int argc_ = 0, char *argv_[] = 0 )
	{
		init();
		for ( int i = 0; i < argc_; i++ )
		{
			string arg( argv_[i] ? argv_[i] : "" );
			if ( arg == "-d" )
				_debug = true;
			if ( arg == "-f" )
				toggle_fullscreen();
			if ( arg == "-s" )
				_to = 0.03;
			if ( arg == "-ss" )
				_to = 0.02;
			if ( arg == "-sss" )
				_to = 0.01;
			if ( arg == "-m" )
				_no_moon = true;
		}
		FPS = lround( 1. / _to );
		Fl::add_timeout( 0.1, cb_timer, this );
		Fl::run();
	}
	bool rising() const
	{
		return _sun_angle >= 180 && _sun_angle < 360.;
	}
	double zenith() const
	{
		return _zenith > 0. ? _zenith > 1. ? 1. : _zenith : .0;
	}
	double nightFactor() const
	{
		static const double NearDist = 2.;
		if ( !_no_moon && rising() )
		{
			double sun_moon_dist = fabs( _sun_angle - _moon_angle );
			if ( sun_moon_dist > 180. )
				sun_moon_dist = 360 - sun_moon_dist;
			if ( sun_moon_dist < NearDist )
				return ( sun_moon_dist / NearDist ) * zenith();
		}
		return zenith();
	}
private:
	bool _debug;
	double _sun_angle;
	int _sun_x;
	int _sun_y;
	double _moon_angle;
	int _moon_x;
	int _moon_y;
	Fl_Color _bg;
	double _zenith;
	double _moon_zenith;
	int _sun_r;
	int _frame;
	double _to;
	bool _hold;
	bool _no_moon;
	double _cloud_dissolve_f;
	vector<Star> _stars;
	vector<Nebula *> _nebula;
	vector<Cloud *> _clouds;
};

int main( int argc_, char *argv_[] )
{
	// optionally start with: -f(ullscreen) -d(ebug) -s[ss](peed)
	srand( time( NULL ) );
	Sunrise s;
	s.run( argc_ - 1, &argv_[1] );
}
