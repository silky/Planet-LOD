﻿////////////////////////////////////////////////////////////////////////////////
//
// Planet-LOD - A simple implementation for rendering planets with LOD
//
// (C) by Sven Forstmann in 2016
//
// Please credit this code if you use it anywhere
//
// License : MIT
// http://opensource.org/licenses/MIT
////////////////////////////////////////////////////////////////////////////////
// Mathlib included from 
// http://sourceforge.net/projects/nebuladevice/
////////////////////////////////////////////////////////////////////////////////
#include "gui.h"
////////////////////////////////////////////////////////////////////////////////
struct World
{
	enum { patch_resolution = 16 };

	static int tris_rendered;
	static GLuint vbo, vboidx, indices ;
	static Shader shader;

	static void begin_patch()
	{
		if (vbo == 0)
		{
			int n = patch_resolution;
			std::vector<float> v;
			std::vector<ushort> idx;
			loopi(0, n + 1)loopj(0, i + 1)
			{
				v << 1.0f - float(i) / float(n);
				v << float(j) / float(n);
				v << 0.0f;

				if (i > 0 && j > 0)
				{
					idx << ushort(v.size() / 3 - 1);
					idx << ushort(v.size() / 3 - 2);
					idx << ushort(v.size() / 3 - 2 - i);

					if (j < i)
					{
						idx << ushort(v.size() / 3 - 2 - i);
						idx << ushort(v.size() / 3 - 1 - i);
						idx << ushort(v.size() / 3 - 1);
					}
				}
			}
			indices = idx.size();// / 3;
			glGenBuffers(1, &vbo); ogl_check_error();
			glBindBuffer(GL_ARRAY_BUFFER, vbo); ogl_check_error();
			glBufferData(GL_ARRAY_BUFFER, 12 * v.size(), &v[0], GL_STATIC_DRAW); ogl_check_error();
			glGenBuffers(1, &vboidx); ogl_check_error();
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboidx); ogl_check_error();
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(short)*idx.size(), &idx[0], GL_STATIC_DRAW); ogl_check_error();
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			shader = Shader("../shader/Patch");
		}
		//Define this somewhere in your header file
		#define BUFFER_OFFSET(i) ((void*)(i))

		glBindBuffer(GL_ARRAY_BUFFER, vbo); ogl_check_error();
		glEnableClientState(GL_VERTEX_ARRAY); ogl_check_error();
		glVertexPointer(3, GL_FLOAT, 12, BUFFER_OFFSET(0)); ogl_check_error();//The starting point of the VBO, for the vertices
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboidx); ogl_check_error();
		shader.begin();
	}
	static void draw_patch(vec3f p1, vec3f p2, vec3f p3)
	{
		shader.setUniform4f("p1", p1.x, p1.y, p1.z, 1);
		shader.setUniform4f("p2", p2.x, p2.y, p2.z, 1);
		shader.setUniform4f("p3", p3.x, p3.y, p3.z, 1);
		glDrawElements(GL_TRIANGLES, indices, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0)); ogl_check_error();//The starting point of the IBO
		//0 and 3 are the first and last vertices
		//glDrawRangeElements(GL_TRIANGLES, 0, 3, 3, GL_UNSIGNED_SHORT, BUFFER_OFFSET(0));   //The starting point of the IBO
		//glDrawRangeElements may or may not give a performance advantage over glDrawElements	
		tris_rendered += patch_resolution*patch_resolution;
	}
	static void end_patch()
	{
		shader.end();
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		ogl_check_error();
	}

	static void draw_triangle(vec3f p1, vec3f p2, vec3f p3)
	{
		glBegin(GL_TRIANGLES);
		glVertex3f(p1.x, p1.y, p1.z);
		glVertex3f(p2.x, p2.y, p2.z);
		glVertex3f(p3.x, p3.y, p3.z);
		glEnd();
		tris_rendered++;
	}

	static void draw_recursive(vec3f p1,vec3f p2,vec3f p3, vec3f center , float size=1)
	{
		float ratio_size = size * gui.screen[0].slider["lod.ratio"].val; // default : 1
		float minsize = gui.screen[0].slider["detail"].val;    // default : 0.01
		vec3f edge_center[3] = { (p1 + p2) / 2, (p2 + p3) / 2, (p3 + p1) / 2 };
		bool edge_test[3]; double angle[3];

		loopi(0, 3)
		{
			double dot = edge_center[i].dot(center);
			angle[i] = acos(clamp(dot, -1, 1));
			edge_test[i] = angle[i] > ratio_size;
		}

		if (min(angle[0], min(angle[1], angle[2])) > M_PI / 2) return;//culling

		if ((edge_test[0] && edge_test[1] && edge_test[2]) || size < minsize)
		{ 
			if (gui.screen[0].checkbox["patches"].checked)
				draw_patch(p1, p2, p3); 
			else
				draw_triangle(p1, p2, p3);
			return; 
		}
		// Recurse
		vec3f p[6] = { p1, p2, p3, edge_center[0], edge_center[1], edge_center[2] };
		int idx[12] = { 0, 3, 5,    5, 3, 4,    3, 1, 4,    5, 4, 2 };
		bool valid[4] = { 1, 1, 1, 1 };

		if (edge_test[0]){ p[3] = p1; valid[0] = 0; } // skip triangle 0 ?
		if (edge_test[1]){ p[4] = p2; valid[2] = 0; } // skip triangle 2 ?
		if (edge_test[2]){ p[5] = p3; valid[3] = 0; } // skip triangle 3 ?

		loopi(0, 4) if (valid[i])
		{
			draw_recursive(
				p[idx[3 * i + 0]].norm(), 
				p[idx[3 * i + 1]].norm(),
				p[idx[3 * i + 2]].norm(),
				center,size/2 );
		}		
	}
	static void draw(vec3f center)
	{
		// create icosahedron
		float t = (1.0 + sqrt(5.0)) / 2.0;

		std::vector<vec3f> p({ 
			{ -1, t, 0 }, { 1, t, 0 }, { -1, -t, 0 }, { 1, -t, 0 },
			{ 0, -1, t }, { 0, 1, t }, { 0, -1, -t }, { 0, 1, -t },
			{ t, 0, -1 }, { t, 0, 1 }, { -t, 0, -1 }, { -t, 0, 1 },
		});
		std::vector<int> idx({ 
			0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11,
			1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 10, 7, 6, 7, 1, 8,
			3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9,
			4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1
		});

		tris_rendered = 0;

		if (gui.screen[0].checkbox["patches"].checked) begin_patch();

		loopi(0, idx.size() / 3)
		{
			draw_recursive(
				p[idx[i * 3 + 0]].norm(), // triangle point 1
				p[idx[i * 3 + 1]].norm(), // triangle point 2
				p[idx[i * 3 + 2]].norm(), // triangle point 3
				center);
		}
		//draw_patch(vec3f(0, 0, 0), vec3f(1, 0, 0), vec3f(0, 1, 0));
		if (gui.screen[0].checkbox["patches"].checked) end_patch();	

		gui.screen[0].label["stats"].text = Gui::String("Triangles: ") + tris_rendered;
	}
};
int World::tris_rendered;
GLuint  World::vbo = 0, World::vboidx, World::indices = 0; Shader  World::shader;
////////////////////////////////////////////////////////////////////////////////
void init_gui()
{
	gui.init( Gui::Flags::CONTEXT_MENU | Gui::Flags::CUSTOM_MOUSE ,
				"../data/gui_global.txt" , 
				"../data/gui_skin.txt");

	// Add simple Renderer
	{
		// render callback function
		auto render_func=[](Gui::Window *window,Gui::Button* control,int index)
		{	
			if((!control) || (!window) || window->get_toggled() ) return;

			Gui::Window &w=*((Gui::Window*) window);
			Gui::Button &b=w.button["canvas"];

			// resize button to window client area
			b.x=w.pad_left;
			b.y=w.pad_up;
			b.sx=w.sx-w.pad_left-w.pad_right;
			b.sy=w.sy-w.pad_up-w.pad_down;

			bool draw = 1;// (b.hover || b.pressed) && (gui.mouse.button[0] || gui.mouse.button[1] || gui.mouse.wheel != 0);
			if(control!=&b)draw=1;
		
			void *fb=b.var.ptr["fbo"]; if(!fb) return;
			FBO &fbo=*(FBO*)fb;

			if(b.sx!=fbo.width || b.sy!=fbo.height ) // resize FBO ?
			{
				fbo.clear();
				fbo.init(b.sx,b.sy);
				b.skin.tex_normal=b.skin.tex_hover=b.skin.tex_selected=fbo.color_tex;
				draw=1;
			}
			if (!draw) return;


			quaternion q(b.var.vec4["rotation"]);
			if(gui.mouse.button[0] & b.pressed)				// rotate by left mouse
			{
				quaternion qx,qy;
				qx.set_rotate_y( (float)gui.mouse.dx/100);
				qy.set_rotate_x(-(float)gui.mouse.dy/100);
				q=qy*qx*q;
				b.var.vec4["rotation"]=vec4f(q.x,q.y,q.z,q.w);
			}

			double z=b.var.number["zoom"];
			if(b.hover)										// zoom by wheel
			{
				z=clamp( z-gui.mouse.wheel*2 , 2,120 );
				b.var.number["zoom"]=z;
				gui.mouse.wheel=0;
			}
			vec4f pos=b.var.vec4["position"];
			if(gui.mouse.button[1] && (b.pressed||b.hover))	// move by middle button
			{
				pos=pos+vec4f((float)gui.mouse.dx*z/40000.0f,(float)gui.mouse.dy*z/40000.0f,0,0);
				b.var.vec4["position"]=pos;
			}

			// render to fbo
			fbo.enable();
			glClearColor(1,1,1,1);
			glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
			glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
			gluPerspective(z, (GLfloat)b.sx/(GLfloat)b.sy, 0.01 , 10.0);
			glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
			glTranslatef(pos.x,pos.y,-pos.z);		// apply movement
			glMultMatrixf(&matrix44(q).m[0][0]);	// apply quaternion rotation
			glColor4f(0,0,0,1);
			glEnable(GL_DEPTH_TEST);
						
			// center of detail
			q.invert();
			vec3f center = matrix44(q).z_component().norm();

			////////////////////
			//
			//  Draw our World
			//
			if (!gui.screen[0].checkbox["solid"].checked)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			World::draw(center);

			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			////////////////////
			//
			// Draw Bounding Box
			//
			glutWireCube(2);
			
			glDisable(GL_DEPTH_TEST);
			glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
			fbo.disable();
			{
				static uint t = timeGetTime(),f=0;
				int tt = timeGetTime(); f++;
				if (tt - t >= 1000)
				{
					std::cout <<  "fps " << f << std::endl;
					f = 0;t=tt;
				}
			}
			
		};

		Gui::Window &w = gui.screen[0];
		w.button["canvas"]=Gui::Button("",20,250);
		w.button["canvas"].skin=Skin("");
		w.button["canvas"].var.vec4["position"]=vec4f(0,0,2.5,0); // vec4
		w.button["canvas"].var.vec4["rotation"]=vec4f(1,0,0,0); // quaternion
		w.button["canvas"].var.number["zoom"]=80; // fov
		w.button["canvas"].var.ptr["fbo"]=0;
		w.button["canvas"].callback_all=render_func;
		w.button["canvas"].callback_init=[](Gui::Window *w,Gui::Button* b,int i) // call before drawing the first time
			{
				if(w)if(b){b->var.ptr["fbo"]=new FBO(100,100);} // 100,100 is just for initialization; will be resized
			};
		w.button["canvas"].callback_exit=[](Gui::Window *w,Gui::Button* b,int i) // called from the button's destructor
			{
				if(w)if(b)if(b->var.ptr["fbo"]){delete(((FBO*)b->var.ptr["fbo"]));}
			};
		int y = 20;
		w.label["detail"] = Gui::Label("Detail : 0.001",20, y, 100, 20);
		w.slider["detail"] = Gui::Slider(0.001, 1, 0.001, 120, y, 100, 20);
		w.slider["detail"].callback_pressed = [](Gui::Window *w, Gui::Button* b, int i) // call before drawing the first time
		{
			if (w)if (b) w->label["detail"].text = Gui::String("Detail : ") + ((Gui::Slider*)b)->val;
		};
		y += 30;
		w.label["lod.ratio"] = Gui::Label("Ratio : 3", 20, y, 100, 20);
		w.slider["lod.ratio"] = Gui::Slider(0.01, 8, 3, 120, y, 100, 20);
		w.slider["lod.ratio"].callback_pressed = [](Gui::Window *w, Gui::Button* b, int i) // call before drawing the first time
		{
			if (w)if (b) w->label["lod.ratio"].text = Gui::String("Ratio : ") + ((Gui::Slider*)b)->val;
		};
		y += 30;
		w.label["Patches"] = Gui::Label("Patches", 20, y, 100, 20);
		w.checkbox["patches"] = Gui::CheckBox("",0,120, y, 16, 16);
		y += 30;
		w.label["Solid"] = Gui::Label("Solid", 20, y, 100, 20);
		w.checkbox["solid"] = Gui::CheckBox("", 0, 120, y, 16, 16);
		y += 30;
		w.label["stats"] = Gui::Label("default", 20, y, 200, 20);
	}
}
////////////////////////////////////////////////////////////////////////////////
void draw_gui()
{
	gui.draw();
}
////////////////////////////////////////////////////////////////////////////////
void exit_gui()
{
	gui.exit();
}
////////////////////////////////////////////////////////////////////////////////
// Glut Callbacks
void KeyDown1Static(int key, int x, int y)           { gui.keyb.key[ key&255 ] =true;  }
void KeyDown2Static(unsigned char key, int x, int y) { gui.keyb.key[ key&255 ] =true;  }
void KeyUp1Static(int key, int x, int y)             { gui.keyb.key[ key&255 ] =false; }
void KeyUp2Static(unsigned char key, int x, int y)   { gui.keyb.key[ key&255 ] =false; }
void MouseMotionStatic (int x,int y){	gui.mouse.x = x;gui.mouse.y = y;}
void MouseButtonStatic(int button_index, int state, int x, int y)
{
	if(button_index<=2) gui.mouse.button[button_index] =  ( state == GLUT_DOWN ) ? true : false;
	if(button_index>=3) gui.mouse.wheel_update+= button_index == 3 ? -1 : 1;
	MouseMotionStatic (x,y);	
}
void OnWindowClose( void ) { exit_gui(); }
////////////////////////////////////////////////////////////////////////////////

void disp(void);
void reshape(int x, int y);
void keyb(unsigned char key, int x, int y);

int main(int argc, char **argv)
{
	
	glutInit(&argc, argv);
	glutInitWindowSize(1024, 768);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);
	glutCreateWindow("PLANET LOD - BY SVEN FORSTMANN in 2016");

	glutIdleFunc(disp);
	glutDisplayFunc(disp);
	glutReshapeFunc(reshape);

	//glutSpecialFunc(&KeyDown1Static);
	//glutSpecialUpFunc(&KeyUp1Static);
	glutKeyboardFunc(KeyDown2Static);
	glutKeyboardUpFunc(KeyUp2Static);
	glutMotionFunc(MouseMotionStatic);
	glutPassiveMotionFunc(MouseMotionStatic);
	glutMouseFunc (MouseButtonStatic);
	glutCloseFunc (OnWindowClose);

	glewInit();
	glEnable(GL_BLEND);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	init_gui();

	glutMainLoop();

	return 0;
}


void disp(void)
{
	glClearColor(1,1,1,0);
	glClear(GL_COLOR_BUFFER_BIT);
	draw_gui();
	glutSwapBuffers();
}

void reshape(int x, int y)
{
	glViewport(0, 0, x, y);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, x, y, 0, -1, 1);
	gui.screen_resolution_x=x;
	gui.screen_resolution_y=y;
}






