#define OLC_PGE_APPLICATION
#define OLC_PGEX_SOUND
#include <vector>
#include <math.h>
#include "olc.h"
#include "olcs.h"
#include "ppm.h"
#define base unsigned short
#define basebits (1<<(4<<sizeof(base)))
#define width 400
#define height 300
#define pinsize 3
#define sample_rate 8000
#define interpolation_steps 512

using namespace std;

struct func{
	int inputs,locals,outputs; // how many the func takes
	void(*f)(int,base*,base*,base*);
	sprite s; // defined in ppm.h
};

vector<func>functions;

struct box{
	int x,y;
	char type; // index to functions
	base*i,*l,*o; // sizes determined by type
	box(int x,int y,char type):x(x),y(y),type(type){
		i=(base*)malloc(functions[type].inputs);
		for(int j=0;j<functions[type].inputs;j++)i[j]=0;
		l=(base*)malloc(functions[type].locals);
		for(int j=0;j<functions[type].locals;j++)l[j]=0;
		o=(base*)malloc(functions[type].outputs);
		// o will be propagated anyway, so we dont bother setting it
	};
};

vector<box>boxes;

struct conn{
	int in,inn,out,outn;
	// indexes to boxes, and box.i/box.o
};

vector<conn>conns;

void add_conn(int in,int inn,int out,int outn){
	conn c;
	for(int i=0;i<conns.size();i++){
		c=conns[i];
		if(c.out==out&&c.outn==outn){
			if(c.in==in&&c.inn==inn){
				conns.erase(conns.begin()+i);
			}else{
				conns[i].in=in;
				conns[i].inn=inn;
			}
			return;
		}
	}
	conns.push_back((conn){in,inn,out,outn});
}

sprite pin_in,pin_out;

bool boxing=0;int oldx,oldy,box_index,pin_index;
bool conn_in=0,conn_out=0;

int t=0;

short newmap[height*width],oldmap[height*width];char font[1<<14],str[256],sp;

int mod(int a,int b){a%=b;return(a>=0?a:b+a);}
int min(int a,int b){return(a>b?b:a);}
int max(int a,int b){return(a<b?b:a);}
int abs(int a){return a>0?a:-a;}

int interpol(int a,int b,int c){return a+(b-a)*c/interpolation_steps;}

// XXX the following are for the parser, and operate on space-separated, null-terminated strings
bool match_string(char*src,char*dest,int n){} // returns true if the nth substring of src is dest
char*get_string(char*src,int n){} // returns the nth substring
int get_int(char*src,int n){} // returns the nth substring, evaluated as an integer

float ostfunk(int nchannel,float globaltime,float timestep){
	// exact order doesnt matter because we loop anyway
	t++;
	base out=0;

	printf("funcs\n");int j=0;
	for(box x : boxes){
		functions[x.type].f(t,x.i,x.l,x.o);
		printf("%i: [",j++); // this is dumb
		for(int i=0;i<functions[x.type].inputs;i++)printf("%i ",x.i[i]);
		printf("] -> [");
		for(int i=0;i<functions[x.type].outputs;i++)printf("%i ",x.o[i]);
		putchar(']');
		putchar(10);

		for(int i=0;i<functions[x.type].inputs;i++)x.i[i]=0;
	}

	printf("conns\n");
	for(conn x : conns){
		printf("%i.%i -> %i.%i\n",x.in,x.inn,x.out,x.outn);
		if(x.in==0&&x.out==0)
			; // or do we?
		else if(x.in==0)
			boxes[x.out-1].i[x.outn] = 0; // XXX get rid of this
		else if(x.out==0)
			out = boxes[x.in-1].o[x.inn];
		else
			boxes[x.out-1].i[x.outn] = boxes[x.in-1].o[x.inn];
	}

	//for(int i=0;i<out>>2;i++)putchar('#');putchar(10);
	return (float)out/basebits;
}

class Game : public olc::PixelGameEngine{public:
	Game(){sAppName="Game";}
	bool OnUserCreate()override{
		for(int x=0;x<height;x++)
		for(int y=0;y<width;y++)
		newmap[x*width+y]=0;
		olc::SOUND::InitialiseAudio(sample_rate);
		olc::SOUND::SetUserSynthFunction(ostfunk);
		render();
		return(1);
	}

	olc::Pixel sga(short color){
	int r = (color&0x0f00)>>4;
	int g = (color&0x00f0);
	int b = (color&0x000f)<<4;
	r=r|r>>4;g=g|g>>4;b=b|b>>4;
	return olc::Pixel(r,g,b);}

	void p(short c,int x,int y){Draw(y,x,sga(c));}

	void pc(char c,int sx,int sy,short fgc,short bgc){
		for(int x=0;x<8;x++)for(int y=0;y<8;y++)
		newmap[(sx*8+x)*width+sy*8+y]=(font[(unsigned char)c*8+x]&(char)(128>>y))?fgc:bgc;
	}

	void ps(const char*c,int sx,int sy,short fgc,short bgc){
		for(int i=0;c[i];i++){
			pc(c[i]==10?' ':c[i],sx,sy++,fgc,bgc);
			if(sy==width||c[i]==10){sy=0;sx++;} // XXX do we want to reset at initial xy?
		}
	}

	/* XXX do we need this
			most likely, but we could easily make it use ps()
	void pi(int c,int sx,int sy,short fgc,short bgc){char
	a[10],b=0;if(c==0){pc('0',sx,sy,fgc,bgc);return;}for(;
	c>0;c/=10)a[b++]=(c%10)+48;for(b--;b>=0;b--)newmap[sx*
	width+sy-b]=(map){(unsigned char)a[b],fgc,bgc};}
	*/

	// ~~~  relevant stuff  ~~~

	void bezier(int x1,int y1,int x2,int y2,short col){
		// interpol
		if( // idk man, it fucks up sometimes
			x1<0||x1>height||
			y1<0||y1>width||
			x2<0||x2>height||
			y2<0||y2>width
		)return;

		int dx[6],dy[6]; // we need to keep track of 8 points for a quadratic bezier curve
		for(int i=0;i<interpolation_steps;i++){
			dx[0]=x1;
			dy[0]=interpol(y1,y2+(y2-y1),i);

			dx[1]=interpol(x1,x2,i);
			dy[1]=interpol(y2,y1,i);

			dx[2]=x2;
			dy[2]=interpol(y1-(y2-y1),y2,i);

			dx[3]=interpol(dx[0],dx[1],i);
			dy[3]=interpol(dy[0],dy[1],i);

			dx[4]=interpol(dx[1],dx[2],i);
			dy[4]=interpol(dy[1],dy[2],i);


			dx[5]=interpol(dx[3],dx[4],i);
			dy[5]=interpol(dy[3],dy[4],i);

			newmap[dx[5]*width+dy[5]]=col;
		}
	}

	void draw_sprite(sprite s,int dx,int dy){ // XXX
		for(int x=0;x<s.x;x++)for(int y=0;y<s.y;y++)
		newmap[(dx+x)*width+dy+y]=s.data[x*s.y+y];
	}

	void draw_conn(conn c){
		if(c.in==0&&c.out==0)
			bezier(
				height/2, pinsize+pinsize/2,
				height/2, width-pinsize-pinsize/2,
				0x0ff0
			);

		if(c.in==0&&c.out)
			bezier(
				height/2, pinsize+pinsize/2,
				boxes[c.out-1].x+pinsize*2*c.outn, boxes[c.out-1].y,
				0x0ff0
			);

		if(c.in&&(c.out==0))
			bezier(
				boxes[c.in-1].x+pinsize*2*c.inn, boxes[c.in-1].y + functions[boxes[c.out-1].type].s.y, // XXX this is not working
				height/2, width-pinsize-pinsize/2,
				0x0ff0
			);

		if(c.in&&c.out)
			bezier(
				boxes[c.in-1].x+pinsize*2*c.inn, boxes[c.in-1].y + functions[boxes[c.out-1].type].s.y,
				boxes[c.out-1].x+pinsize*2*c.outn, boxes[c.out-1].y,
				0x0ff0
			);
	}

	void refresh(){
		for(int x=0;x<height;x++)
		for(int y=0;y<width;y++)
		if(newmap[x*width+y]!=oldmap[x*width+y]){
			p(newmap[x*width+y],x,y);
			oldmap[x*width+y]=newmap[x*width+y];
		}
	}

	void render(){
		// fancy background
		for(int x=0;x<height;x++)for(int y=0;y<width;y++)
		//newmap[x*width+y]=(x^y)&1?0x0444:0x0bbb;
		newmap[x*width+y]=((x^y)>>3&3)*0x111;

		draw_sprite(pin_out,(height-pinsize)/2,pinsize);
		draw_sprite(pin_in,(height-pinsize)/2,width-pinsize*2);

		for(box b : boxes){
			draw_sprite(functions[b.type].s,b.x,b.y);
			if(b.type==0){
				for(int y=b.y;y<b.y+functions[b.type].s.y;y++)
				newmap[(
					b.x+(basebits-b.l[0])*functions[b.type].s.x/basebits
				)*width+y]=0x0b88;
			}else if(b.type==7){
				for(int i=0;i<32;i++){
					// XXX draw data
				}
			}


			for(int i=0;i<functions[b.type].inputs;i++)
				draw_sprite(pin_in,b.x+pinsize*2*i,b.y-pinsize);
			for(int i=0;i<functions[b.type].outputs;i++)
				draw_sprite(pin_out,b.x+pinsize*2*i,b.y+functions[b.type].s.y);
		}

		for(conn c : conns)
			draw_conn(c);

		ps(str,height/8-1,0,0x0f44,0);

	}

	void step(char key){}

	bool OnUserUpdate(float fElapsedTime)override{
		int x=GetMouseY(),y=GetMouseX();
		if(GetKey(olc::ESCAPE).bPressed)return 0;

		if(GetMouse(1).bHeld){
			for(int i=0;i<boxes.size();i++){
				if(
					x>boxes[i].x&&
					y>boxes[i].y&&
					x<boxes[i].x+functions[boxes[i].type].s.x&&
					y<boxes[i].y+functions[boxes[i].type].s.y
				){
					if(boxes[i].type==0)
						boxes[i].l[0]=basebits-(x-boxes[i].x)*basebits/functions[boxes[i].type].s.x;
					else if(boxes[i].type==7)
						boxes[i].l[(y-boxes[i].y)*32/64]= (x-boxes[i].x)*basebits/16;

					// basekit (6) has a bunch of inputs

					// seqdata (7) has inputs

				}
			}
		}

		// ~~~  text controls  ~~~

		if(GetKey(olc::A).bPressed)str[sp++]='a';
		if(GetKey(olc::B).bPressed)str[sp++]='b';
		if(GetKey(olc::C).bPressed)str[sp++]='c';
		if(GetKey(olc::D).bPressed)str[sp++]='d';
		if(GetKey(olc::E).bPressed)str[sp++]='e';
		if(GetKey(olc::F).bPressed)str[sp++]='f';
		if(GetKey(olc::G).bPressed)str[sp++]='g';
		if(GetKey(olc::H).bPressed)str[sp++]='h';
		if(GetKey(olc::I).bPressed)str[sp++]='i';
		if(GetKey(olc::J).bPressed)str[sp++]='j';
		if(GetKey(olc::K).bPressed)str[sp++]='k';
		if(GetKey(olc::L).bPressed)str[sp++]='l';
		if(GetKey(olc::M).bPressed)str[sp++]='m';
		if(GetKey(olc::N).bPressed)str[sp++]='n';
		if(GetKey(olc::O).bPressed)str[sp++]='o';
		if(GetKey(olc::P).bPressed)str[sp++]='p';
		if(GetKey(olc::Q).bPressed)str[sp++]='q';
		if(GetKey(olc::R).bPressed)str[sp++]='r';
		if(GetKey(olc::S).bPressed)str[sp++]='s';
		if(GetKey(olc::T).bPressed)str[sp++]='t';
		if(GetKey(olc::U).bPressed)str[sp++]='u';
		if(GetKey(olc::V).bPressed)str[sp++]='v';
		if(GetKey(olc::W).bPressed)str[sp++]='w';
		if(GetKey(olc::X).bPressed)str[sp++]='x';
		if(GetKey(olc::Y).bPressed)str[sp++]='y';
		if(GetKey(olc::Z).bPressed)str[sp++]='z';
		if(GetKey(olc::K1).bPressed)str[sp++]='1';
		if(GetKey(olc::K2).bPressed)str[sp++]='2';
		if(GetKey(olc::K3).bPressed)str[sp++]='3';
		if(GetKey(olc::K4).bPressed)str[sp++]='4';
		if(GetKey(olc::K5).bPressed)str[sp++]='5';
		if(GetKey(olc::K6).bPressed)str[sp++]='6';
		if(GetKey(olc::K7).bPressed)str[sp++]='7';
		if(GetKey(olc::K8).bPressed)str[sp++]='8';
		if(GetKey(olc::K9).bPressed)str[sp++]='9';
		if(GetKey(olc::K0).bPressed)str[sp++]='0';
		if(GetKey(olc::SPACE).bPressed)str[sp++]=' ';
		if(GetKey(olc::BACK).bPressed && sp>0)str[--sp]=0;

		if(GetKey(olc::ENTER).bPressed){
			// XXX parse str
			//boxes.push_back(box(x,y,0));

			if(str[0]>='0' && str[0]<='9' && str[0]-'0'<functions.size())
				boxes.push_back(box(x,y,str[0]-'0'));

			if(str[0]=='q')return 0;

			if(str[0]=='e'){ // erase box
				for(int i=0;i<boxes.size();i++){
					if(
						x>boxes[i].x&&
						y>boxes[i].y&&
						x<boxes[i].x+functions[boxes[i].type].s.x&&
						y<boxes[i].y+functions[boxes[i].type].s.y
					){
						boxes.erase(boxes.begin()+i);
						// XXX also erase all conns that connect to box
						for(int c=0;c<conns.size();c++){
							if(conns[c].in==i||conns[c].out==i)conns.erase(conns.begin()+c);
							else{
								if(conns[c].in>i)conns[c].in--;
								if(conns[c].out>i)conns[c].out--;
							}
						}
					}
				}
			}

			for(int i=0;i<256;i++)str[i]=0;sp=0;
		}

		// ~~~  mouse controls  ~~~

		if(GetMouse(0).bPressed){

			if(y>pinsize&&x>(height-pinsize)/2&&y<pinsize*2&&x<(height+pinsize)/2){
				oldx=x,oldy=y;
				box_index=pin_index=0;
				conn_out=1;
			}

			if(y>width-pinsize*2&&x>(height-pinsize)/2&&y<width-pinsize&&x<(height+pinsize)/2){
				oldx=x,oldy=y;
				box_index=pin_index=0;
				conn_in=1;
			}

			//if(!(conn_in||conn_out))
			for(int i=0;i<boxes.size();i++){

				//if(!(conn_in||conn_out)){

				for(int in=0;in<functions[boxes[i].type].inputs;in++)
					if(
						x>boxes[i].x+pinsize*2*in &&
						y>boxes[i].y-pinsize &&
						x<boxes[i].x+pinsize*2*in+pinsize &&
						y<boxes[i].y
					){
						oldx=x;oldy=y;
						box_index=i+1;pin_index=in;
						conn_in=1;
					}

				for(int out=0;out<functions[boxes[i].type].outputs;out++)
					if(
						x>boxes[i].x+pinsize*2*out &&
						y>boxes[i].y+functions[boxes[i].type].s.y &&
						x<boxes[i].x+pinsize*2*out+pinsize &&
						y<boxes[i].y+functions[boxes[i].type].s.y+pinsize
					){
						oldx=x;oldy=y;
						box_index=i+1;pin_index=out;
						conn_out=1;
					}
				//}

				if(//!(conn_in||conn_out)&&
					x>boxes[i].x&&
					y>boxes[i].y&&
					x<boxes[i].x+functions[boxes[i].type].s.x&&
					y<boxes[i].y+functions[boxes[i].type].s.y
				){
					boxing=1;box_index=i;
					oldx=x,oldy=y;
				}
			}
		}

		if(GetMouse(0).bHeld){
			if(boxing){
				boxes[box_index].x+=x-oldx;
				boxes[box_index].y+=y-oldy;
				oldx=x,oldy=y;
			}
		}

		if(GetMouse(0).bReleased){

			if((conn_in&&y>pinsize&&x>(height-pinsize)/2&&y<pinsize*2&&x<(height+pinsize)/2))
				add_conn(0,0,box_index,pin_index);

			if((conn_out&&y>width-pinsize*2&&x>(height-pinsize)/2&&y<width-pinsize&&x<(height+pinsize)/2))
				add_conn(box_index,pin_index,0,0);

			for(int i=0;i<boxes.size();i++){

				if(conn_out)
				for(int in=0;in<functions[boxes[i].type].inputs;in++)
					if(
						x>boxes[i].x+pinsize*2*in &&
						y>boxes[i].y-pinsize &&
						x<boxes[i].x+pinsize*2*in+pinsize &&
						y<boxes[i].y
					)
						add_conn(box_index,pin_index,i+1,in);

				if(conn_in)
				for(int out=0;out<functions[boxes[i].type].outputs;out++)
					if(
						x>boxes[i].x+pinsize*2*out &&
						y>boxes[i].y+functions[boxes[i].type].s.y &&
						x<boxes[i].x+pinsize*2*out+pinsize &&
						y<boxes[i].y+functions[boxes[i].type].s.y+pinsize
					)
						add_conn(i+1,out,box_index,pin_index);
			}

			// XXX iterate, remove doubles

			boxing=conn_in=conn_out=0;
		}

		// XXX text input and parser

		render();

		if(GetMouse(0).bHeld){ // this is a nightmare
			if(conn_in)bezier(oldx,oldy,x,y,0x0f0f);
			if(conn_out)bezier(x,y,oldx,oldy,0x0f0f);
		}

		refresh();

		return 1;
	}
};

int main(){
	FILE*f=fopen("font","r");for(int i=0;i
	<3200;i++)*(font+i)=getc(f);fclose(f);

	printf("%i\n",basebits);
	pin_in=(sprite){pinsize,pinsize,(short*)malloc(pinsize*pinsize*sizeof(short))};
	pin_out=(sprite){pinsize,pinsize,(short*)malloc(pinsize*pinsize*sizeof(short))};
	for(int i=0;i<pinsize*pinsize;i++){pin_in.data[i]=0xf0;pin_out.data[i]=0xf00;}

	// constant (adjustable)
	functions.push_back((func){0,1,1,[](int t,base*in,base*loc,base*out){out[0]=loc[0]*1024/basebits;},loadimg("ui/const_var.ppm")});

	// sine oscillator
	functions.push_back((func){1,0,1,[](int t,base*in,base*loc,base*out){out[0]=(base)(sinf((float)t*in[0]*2*M_PI/sample_rate)*(basebits/2-1)+basebits/2);},loadimg("ui/sine.ppm")});

	// adder
	functions.push_back((func){2,0,1,[](int t,base*in,base*loc,base*out){out[0]=(in[0]+in[1])/2;},loadimg("ui/add.ppm")});

	// multiplier
	functions.push_back((func){2,0,1,[](int t,base*in,base*loc,base*out){out[0]=in[0]*in[1]/basebits;},loadimg("ui/mult.ppm")});

	// demod
	functions.push_back((func){1,0,1,[](int t,base*in,base*loc,base*out){out[0]=in[0]*100/basebits;},loadimg("ui/const.ppm")});

	// blend XXX
	functions.push_back((func){3,0,1,[](int t,base*in,base*loc,base*out){out[0]=(in[0]*in[2]+in[1]*(basebits-in[2]))/basebits;},loadimg("ui/const.ppm")});

	// drumkit
	functions.push_back((func){0,65,1,[](int t,base*in,base*loc,base*out){
		// first sample tells us sampling speed
		// first set of 32 tell us the amplitude
		// second set tell us the frequency
		int sample = t*in[0]/16,interp=(t*in[0])%16;
		base freq=(
			loc[sample%32+1]*(16-interp) +
			loc[(sample+1)%32+1]*interp
		)/16;
		base amp=(
			loc[sample%32+33]*(16-interp) +
			loc[(sample+33)%32+1]*interp
		)/16;

		out[0]=(base)(amp*sinf((float)t*freq*2*M_PI/sample_rate)*(basebits/2-1)+basebits/2); // XXX normalize?

	},loadimg("ui/drumkit.ppm")});

	// sequential
	functions.push_back((func){1,32,1,[](int t,base*in,base*loc,base*out){
		int
			sample=t*in[0]*16/basebits,
			inter=t*in[0]/basebits %16; // XXX should be

		out[0]=
			loc[(sample)%32]*(16-inter) +
			loc[(sample+1)%32]*(inter);

	},loadimg("ui/seqdata.ppm")});



	// XXX etc
	Game game;if(game.Construct(width,height,3,3))game.Start();return(0);
}

/*
have arrays of those
draw boxes
draw connections w/ bezier curves
	control points going horizontally to half/full (?) the distance to the other one
the value of the conns will be used for propagation, since, as it is stored separately from the functions and the conns loop, it becomes irrelevant what order they are executed in and the output is the same (w/in some discreptancy when new ones are created)
have a vi-like text console at the top/bottom. all text input gets redirected there. when enter is pressed, it gets executed, spawning new objects (or saving/loading etc)
left side of screen has a single output (timestamp), right side a single output (audio jack)
how will hardcoded values be treated? i was thinking a single "function" box with no inputs and one output, but where will the actual value be stored?
	also the sprite is just a rectangle w/ the value in it

draw ports separately from sprites

conns attached to 0 lead to input/output

we might want to implement timestamp ourselves - will also allow us to reset it
also, going by the olcs specs, we could have multiple output channels!

olcpge defines resourcepacks, we might want to use those for function sprites
either that, or we implement our own
sigh. get the ppm.h

tracks will need us to be able to write to them
	fffuuuck, more interface bullshit!

also, t'would be cool to have a function that allows us to hardcode a waveform

we declare sample rate at audio setup
	alternatively, globaltimetells us the current time (in seconds - its a float)

when propagating: first set all inputs to 0, then sum them
	might also wanna make counters for normalizing them afterwards...

also what will we do with double conns?
	add an equality operator to the conns (or write it out, well only need it once), and when we add a new one, go thru list, and if any element is equal to the last, pop one from list and discard
	or do we want to use that to delete them? iow if we draw a track that already exists, delete that track?
	actually, itll make a lot more sense if we only allow one connection per input. so if we find a different connection that still leads to the same input, we delete it (and if its also from the same output, we delete ourselves as well)

make a single box, connect its output to the global output. why does it fuck up?
	it fixes itself once we add more boxes, but why

XXX how do we initialize and display local box data?
	will be relevant for constants
	sliders? sliders are cool, even if imprecise
	also, in that case, we can link stuff to them as- nope! we are NOT doing that.

make the bezier curves have a direction
and rewrite code to draw them differently, depending on which way they are facing

ok so the idea with providing global time as an input? didn't work.
alternative method: provide it as a global variable, and each function just references it?
might be easier to pass it as input...
XXX remove global input
	FUCK, well also have to deal with indexes...

XXX adder and multiplier (should) have variable number of inputs

we currently have 2 zeroes, should we maybe use a signed basetype?

do we declare inputs as 0 only on box creation (as we currently do), or do we do it every cycle (after applying but before propagating)?

separate workspace for editing stuff, also box selection

some functions will require [] sprites, how will we handle that?

do we wanna have the parser in a separate file? its quite a lot of stuff...

saving

deleting boxes

idea: each function also gets a function which takes as input the mouse position within the box

drum kit
	speed, amplitude, frequency
	32 samples?
	sounds a bit too fucked...

sequential data
	one input (read speed), one output
	then we can have the drum kit have 3 inputs and one output

*/
