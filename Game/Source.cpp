
#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if defined(__APPLE__)
#include <GLUT/GLUT.h>
#include <OpenGL/gl3.h>
#include <OpenGL/glu.h>
#else
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#endif
#include <GL/glew.h>		 
#include <GL/freeglut.h>	
#endif

#include <string>
#include <vector>

unsigned int windowWidth = 800, windowHeight = 800;
unsigned char keyPressed[256];
float mouseX;
float mouseY;

// OpenGL major and minor versions
int majorVersion = 3, minorVersion = 0;

void getErrorInfo(unsigned int handle)
{
	int logLen;
	glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &logLen);
	if (logLen > 0)
	{
		char * log = new char[logLen];
		int written;
		glGetShaderInfoLog(handle, logLen, &written, log);
		printf("Shader log:\n%s", log);
		delete log;
	}
}

// check if shader could be compiled
void checkShader(unsigned int shader, char * message)
{
	int OK;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &OK);
	if (!OK)
	{
		printf("%s!\n", message);
		getErrorInfo(shader);
	}
}

// check if shader could be linked
void checkLinking(unsigned int program)
{
	int OK;
	glGetProgramiv(program, GL_LINK_STATUS, &OK);
	if (!OK)
	{
		printf("Failed to link shader program!\n");
		getErrorInfo(program);
	}
}

// vertex shader for textured quads
const char *vertexSource0 = R"( 
	#version 130 
    	precision highp float; 
	
	in vec2 vertexPosition; 
	in vec2 vertexTexCoord; 
	uniform mat4 MVP; 
	out vec2 texCoord; 
	
	void main() { 
		texCoord = vertexTexCoord; 
		gl_Position = vec4(vertexPosition.x, vertexPosition.y, 0, 1) * MVP; 
	} 
)";

// fragment shader for textured quads
const char *fragmentSource0 = R"( 
	#version 130 
    	precision highp float; 
	
	uniform sampler2D samplerUnit; 
	in vec2 texCoord; 
	out vec4 fragmentColor; 
	
	void main() { 
		fragmentColor = texture(samplerUnit, texCoord); 
	} 
)";

// vertex shader for textured quads
const char *vertexSource1 = R"( 
	precision highp float; 
	in vec2 vertexPosition; 
	in vec3 vertexColor; 
	uniform mat4 MVP; 
	out vec3 color; 
	void main() {
	color = vertexColor; 
	gl_Position = vec4(vertexPosition.x, 	vertexPosition.y, 0, 1) * MVP; 
	} 

)";

// fragment shader for textured quads
const char *fragmentSource1 = R"( 
	#version 130 
	precision highp float; 
	in vec3 color; 
	out vec4 fragmentColor; 
	void main() { 
	fragmentColor = vec4(color.x, color.y, 	color.z,  0);
	} 
)";

// row-major matrix 4x4
struct mat4
{
	float m[4][4];
public:
	mat4() {}
	mat4(float m00, float m01, float m02, float m03,
		float m10, float m11, float m12, float m13,
		float m20, float m21, float m22, float m23,
		float m30, float m31, float m32, float m33)
	{
		m[0][0] = m00; m[0][1] = m01; m[0][2] = m02; m[0][3] = m03;
		m[1][0] = m10; m[1][1] = m11; m[1][2] = m12; m[1][3] = m13;
		m[2][0] = m20; m[2][1] = m21; m[2][2] = m22; m[2][3] = m23;
		m[3][0] = m30; m[3][1] = m31; m[3][2] = m32; m[3][3] = m33;
	}

	mat4 operator*(const mat4& right)
	{
		mat4 result;
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				result.m[i][j] = 0;
				for (int k = 0; k < 4; k++) result.m[i][j] += m[i][k] * right.m[k][j];
			}
		}
		return result;
	}
	operator float*() { return &m[0][0]; }
};

// 3D point in homogeneous coordinates
struct vec4
{
	float v[4];

	vec4(float x = 0, float y = 0, float z = 0, float w = 1)
	{
		v[0] = x; v[1] = y; v[2] = z; v[3] = w;
	}

	vec4 operator*(const mat4& mat)
	{
		vec4 result;
		for (int j = 0; j < 4; j++)
		{
			result.v[j] = 0;
			for (int i = 0; i < 4; i++) result.v[j] += v[i] * mat.m[i][j];
		}
		return result;
	}

	vec4 operator+(const vec4& vec)
	{
		vec4 result(v[0] + vec.v[0], v[1] + vec.v[1], v[2] + vec.v[2], v[3] + vec.v[3]);
		return result;
	}
};


struct vec2
{
	float x, y;

	vec2(float x = 0.0, float y = 0.0) : x(x), y(y) {}

	static vec2 random()
	{
		return vec2(
			((float)rand() / RAND_MAX) * 2 - 1,
			((float)rand() / RAND_MAX) * 2 - 1);
	}

	vec2 operator+(const vec2& v)
	{
		return vec2(x + v.x, y + v.y);
	}

	vec2 operator-(const vec2& v)
	{
		return vec2(x - v.x, y - v.y);
	}

	vec2 operator*(float s)
	{
		return vec2(x * s, y * s);
	}

	float length() { return sqrt(x * x + y * y); }
};




// shader program IDs
unsigned int shaderProgram0;
unsigned int shaderProgram1; // will be used for non-textured quads
unsigned int shaderProgram2; // will be used for explosions


enum OBJECT_TYPE { FIREBALL, LANDER, PLATFORM, QUAD, LIFE, 
	DIAMOND, DIAMONDCOUNT, AFTERBURNER, POKEBALL, SKUNTANK,
	FLAMETHROWER, BG};

class Object {
protected:
	bool stillAlive;
	vec2 position, scale;
	float orientation;

	float angularVelocity;

	unsigned int vao;
	unsigned int shader;

public:
	Object(unsigned int sp) : scale(1.0, 1.0), orientation(0.0), stillAlive(true), angularVelocity(0.0), shader(sp) { }

	vec2 velocity;
	void Destroy() { stillAlive = false; }
	bool StillAlive() { return stillAlive; }
	vec2 Velocity() { return velocity; }
	vec2 Scale() { return scale; }
	float AngularVelocity() { return angularVelocity; };

	void SetTransform()
	{
		mat4 scale(scale.x, 0, 0, 0,
			0, scale.y, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);

		float alpha = orientation / 180 * M_PI;
		mat4 rotate(cos(alpha), sin(alpha), 0, 0,
			-sin(alpha), cos(alpha), 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);

		mat4 translate(1, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			position.x, position.y, 0, 1);


		mat4 view((float)windowHeight / windowWidth, 0, 0, 0,
			0, 1, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1);


		mat4 MVPTransform = scale * rotate * translate * view;
		int location = glGetUniformLocation(shader, "MVP");
		if (location >= 0) glUniformMatrix4fv(location, 1, GL_TRUE, MVPTransform);
		else printf("uniform MVPTransform cannot be set\n");
	}

	virtual void Draw()
	{
		glUseProgram(shader);
		SetTransform();
		DrawModel();
	}

	virtual void DrawModel() = 0;

	virtual void Move(float dt)
	{
		position = position + velocity * dt;

		orientation = orientation + angularVelocity * dt;
	}

	virtual void Control()
	{

	}

	virtual void Interact(Object*)
	{

	}

	virtual OBJECT_TYPE GetType() = 0;

	virtual bool TooClose(Object* o)
	{
		if ((position - o->position).length() < 0.15) return true;
		return false;
	}

	vec2 GetPosition()
	{
		return position;
	}
};


class Quad : public Object {
public:
	Quad() : Object(shaderProgram1) {
		// NOTE THAT shaderProgram1 IS NOT A VALID SHADER ID NOW, IT HAS TO BE INITIALIZED SIMILARLY AS shaderProgram0 IN onInitialization!

		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		unsigned int vbo[2];
		glGenBuffers(2, &vbo[0]);

		// vertex coordinates: vbo[0] -> Attrib Array 0 -> vertexPosition of the vertex shader
		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		static float vertexCoords[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, 0.5 };
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		// vertex colors: vbo[1] -> Attrib Array 1 -> vertexColor of the vertex shader
		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		static float vertexColors[] = { 1, 0, 0,    0, 1, 0,    0, 0, 1,    1, 1, 1 };
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexColors), vertexColors, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	virtual void DrawModel() {
		glBindVertexArray(vao);
		glDrawArrays(GL_QUADS, 0, 4);
	}

	OBJECT_TYPE GetType() { return QUAD; }
};


extern "C" unsigned char* stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);

class Texture
{
	unsigned int textureId;

public:
	Texture(const std::string& inputFileName)
	{
		unsigned char* data;
		int width; int height; int nComponents = 4;

		data = stbi_load(inputFileName.c_str(), &width, &height, &nComponents, 0);

		if (data == NULL)
		{
			return;
		}

		glGenTextures(1, &textureId);
		glBindTexture(GL_TEXTURE_2D, textureId);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		delete data;
	}

	void Bind(unsigned int shader)
	{
		int samplerUnit = 0;
		int location = glGetUniformLocation(shader, "samplerUnit");
		glUniform1i(location, samplerUnit);
		glActiveTexture(GL_TEXTURE0 + samplerUnit);
		glBindTexture(GL_TEXTURE_2D, textureId);
	}
};


class TexturedQuad : public Object
{
	Texture *texture;

public:

	TexturedQuad(Texture* t, unsigned int sp = shaderProgram0) : Object(sp), texture(t)
	{
		glGenVertexArrays(1, &vao);
		glBindVertexArray(vao);

		unsigned int vbo[2];
		glGenBuffers(2, &vbo[0]);

		glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
		static float vertexCoords[] = { -0.5, 0.5, 0.5, 0.5, 0.5, -0.5, -0.5, -0.5 };
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexCoords), vertexCoords, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);


		glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
		static float vertexTexCoord[] = { 0, 0,  1, 0,  1, 1,  0, 1 };
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertexTexCoord), vertexTexCoord, GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	}

	virtual void DrawModel()
	{
		texture->Bind(shader);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBindVertexArray(vao);
		glDrawArrays(GL_QUADS, 0, 4);
		glDisable(GL_BLEND);
	}
};

boolean landed = false;
int lives = 3;
boolean keyDown = false;

vec2 posn;

class Afterburner : public TexturedQuad
{
	Texture *texture = new Texture("afterburner.png");

public:

	Afterburner(Texture* t) : TexturedQuad(t)
	{
		scale = vec2(0.3, 0.3);
		position = vec2(posn.x, posn.y - 0.1);
	}

	OBJECT_TYPE GetType() { return AFTERBURNER; }

	void Control()
	{
		position = vec2(posn.x -0.04, posn.y - 0.25);
	}

	virtual void DrawModel()
	{
		if (keyDown) {
			texture->Bind(shader);

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBindVertexArray(vao);
			glDrawArrays(GL_QUADS, 0, 4);
			glDisable(GL_BLEND);
		}
	}
};

class Lander : public TexturedQuad
{

public:

	Lander(Texture* t) : TexturedQuad(t)
	{
		scale = vec2(0.3, 0.3);
		position = vec2(0.0, 0.7);
	}

	void Control()
	{
		if (lives == 0) Destroy();
		if (!landed) {

			angularVelocity = 0.0;
			
			if (keyPressed['a']) {
				velocity = velocity + vec2(-0.01, 0);
				angularVelocity += 20.0;
			}
			if (keyPressed['d']) {
				velocity = velocity + vec2(0.01, 0);
				angularVelocity -= 20.0;
			}
			if (keyPressed['w']) {
				velocity = velocity + vec2(0, 0.01);
			}
			if (keyPressed['s']) {
				velocity = velocity + vec2(0, -0.01);
			}
			velocity = velocity + vec2(0, -0.003);
		}
		else {
			velocity = vec2(0.0, 0.0);
			angularVelocity = 0.0;
		}
		posn = GetPosition();
	}

	void Interact(Object* o)
	{
		if (o->GetType() == FIREBALL)
		{
			if (TooClose(o))
			{
				o->Destroy();
				lives -= 1;
			}
		}
	}

	OBJECT_TYPE GetType() { return LANDER; }
};

class Life : public TexturedQuad
{
public:

	Life(Texture* t, int place) : TexturedQuad(t)
	{
		scale = vec2(0.1, 0.1);
		position = vec2(-1 + (place *.1), .75);
	}

	OBJECT_TYPE GetType() { return LIFE; }
};

class DiamondCount : public TexturedQuad
{
public:

	DiamondCount(Texture* t, int place) : TexturedQuad(t)
	{
		scale = vec2(0.1, 0.1);
		position = vec2(1 - (place *.1), .75);
	}

	OBJECT_TYPE GetType() { return DIAMONDCOUNT; }
};

boolean newDiamond = false;
int diamonds = 0;

class Diamond : public TexturedQuad
{
public:

	Diamond(Texture* t) : TexturedQuad(t)
	{
		scale = vec2(0.05, 0.05);
		position = vec2::random();
		velocity = vec2(0.0, -0.1);
	}

		virtual void Move(float dt)
	{
		Object::Move(dt);
		if (position.x < -2) position.x = 2;
		if (position.x >  2) position.x = -2;
		if (position.y < -2) position.y = 2;
		if (position.y >  2) position.y = -2;
	}

	OBJECT_TYPE GetType() { return DIAMOND; }

	void Interact(Object* o)
	{
		if (o->GetType() == LANDER)
		{
			if (TooClose(o))
			{
				Destroy();
				newDiamond = true;
				diamonds += 1;
			}
		}
	}
};

class Fireball : public TexturedQuad
{

public:
	Fireball(Texture* t) : TexturedQuad(t)
	{
		scale = vec2(0.1, 0.1);
		velocity = vec2::random();
		//orientation = atan(velocity.y / velocity.x);
		position = vec2::random();
	}

	OBJECT_TYPE GetType() { return FIREBALL; }

	virtual void Move(float dt)
	{
		Object::Move(dt);
		if (position.x < -2) position.x = 2;
		if (position.x >  2) position.x = -2;
		if (position.y < -2) position.y = 2;
		if (position.y >  2) position.y = -2;
	}
};

class Platform : public TexturedQuad
{

public:
	Platform(Texture* t) : TexturedQuad(t)
	{
		scale = vec2(0.5, 0.1);
		position = vec2(0.5, -0.9);
	}

	OBJECT_TYPE GetType() { return PLATFORM; }

	void Interact(Object* o)
	{
		if (o->GetType() == LANDER)
		{
			if (TooClose(o))
			{
				if (o->Velocity().y > -0.5) landed = true;
				else o->Destroy();
			}
		}
	}
};

class PlatformEnd : public TexturedQuad
{

public:
	PlatformEnd(Texture* t, vec2 posn, vec2 platformscale, int side) : TexturedQuad(t)
	{
		scale = vec2(0.1, platformscale.y);
		if (side == 1) { 
			position = vec2(posn.x + .3, posn.y);
			scale = vec2(-0.1, platformscale.y);
		}
		else position = vec2(posn.x - .3, posn.y);
		
	}

	OBJECT_TYPE GetType() { return PLATFORM; }
};

class Flipper : public TexturedQuad
{
public:
	Flipper(Texture* t) : TexturedQuad(t)
	{
		scale = vec2(0.5, 0.1);
		position = vec2(-0.5, -0.7);
	}

	OBJECT_TYPE GetType() { return PLATFORM; }

	void Interact(Object* o)
	{
		if (o->GetType() == LANDER)
		{
			if (TooClose(o))
			{
				vec2 old = o->velocity;
				o->velocity = vec2(old.x, old.y * -1);
			}
		}
	}
};

boolean caught = false;

class Pokeball : public TexturedQuad
{
public:
	Pokeball(Texture* t, vec2 posn) : TexturedQuad(t)
	{
		scale = vec2(0.1, 0.1);
		position = posn;
		
		float mx = ((mouseX - (windowWidth/2))/1000) *2;
		float my = -((mouseY - (windowHeight/2))/1000) *2;

		vec2 click = vec2(mx, my);
		
		float distance = sqrt(pow(click.x - posn.x, 2) + pow(click.y - posn.y, 2));
		float directionX = (click.x - posn.x) / distance;
		float directionY = (click.y - posn.y) / distance;

		velocity = vec2(directionX, directionY);
	}

	OBJECT_TYPE GetType() { return POKEBALL; }

	void Interact(Object* o)
	{
		if (o->GetType() == SKUNTANK)
		{
			if (TooClose(o)) {
				caught = true;
				Destroy();
			}			
		}
	}

	virtual void Move(float dt)
	{
		Object::Move(dt);
		if (position.x < -1) Destroy();
		if (position.x >  1) Destroy();
		if (position.y < -1) Destroy();
		if (position.y >  1) Destroy();
	}
};

class FlameThrower : public TexturedQuad
{
public:
	FlameThrower(Texture* t, vec2 posn) : TexturedQuad(t)
	{
		scale = vec2(0.1, 0.1);
		position = posn;

		float mx = ((mouseX - (windowWidth / 2)) / 1000) * 2;
		float my = -((mouseY - (windowHeight / 2)) / 1000) * 2;

		vec2 click = vec2(mx, my);

		float distance = sqrt(pow(click.x - posn.x, 2) + pow(click.y - posn.y, 2));
		float directionX = (click.x - posn.x) / distance;
		float directionY = (click.y - posn.y) / distance;

		velocity = vec2(directionX, directionY);
	}

	OBJECT_TYPE GetType() { return FLAMETHROWER; }

	virtual void Move(float dt)
	{
		Object::Move(dt);
		if (position.x < -1) Destroy();
		if (position.x >  1) Destroy();
		if (position.y < -1) Destroy();
		if (position.y >  1) Destroy();
	}
};

class Skuntank : public TexturedQuad
{
public:
	Skuntank(Texture* t) : TexturedQuad(t)
	{
		scale = vec2(0.3, 0.3);
		position = vec2(-0.3, -0.6);
	}

	OBJECT_TYPE GetType() { return SKUNTANK; }

	void Interact(Object* o)
	{
		if (o->GetType() == POKEBALL)
		{
			if (TooClose(o)) {
				Destroy();
			}
		}
	}

};

boolean mouseClicked = false;
int lastTime = 0;

class Scene
{
	std::vector<Texture*> textures;
	std::vector<Object*> objects;
	Lander* lander;
	Platform* platform;

public:
	Scene()
	{
		lander = 0;
	}

	void Initialize()
	{
		textures.push_back(new Texture("platform.png"));
		textures.push_back(new Texture("lander.png"));
		textures.push_back(new Texture("fireball.png"));
		textures.push_back(new Texture("diamond.png"));
		textures.push_back(new Texture("afterburner.png"));
		textures.push_back(new Texture("platformend.png"));
		textures.push_back(new Texture("pokeball.png"));
		textures.push_back(new Texture("skun.png"));
		
		objects.push_back(platform = new Platform(textures[0]));
		objects.push_back(new Flipper(textures[0]));
		objects.push_back(new PlatformEnd(textures[5], platform->GetPosition(), 
			platform->Scale(), 1));
		objects.push_back(new PlatformEnd(textures[5], platform->GetPosition(),
			platform->Scale(), -1));
		lander = new Lander(textures[1]);
		objects.push_back(lander);
		objects.push_back(new Skuntank(textures[7]));
		objects.push_back(new Afterburner(textures[4]));
		

		for (int i = 0; i < 10; i++) objects.push_back(new Fireball(textures[2]));
		for (int i = 0; i < 10; i++) objects.push_back(new Diamond(textures[3]));

		for (int i = 1; i <= lives; i++) {
			Life* life = new Life(textures[1], i);
			objects.push_back(life);
		}
	}

	~Scene()
	{
		for (int i = 0; i < textures.size(); i++) delete textures[i];
		for (int i = 0; i < objects.size(); i++) delete objects[i];
	}

	void Draw()
	{
		for (int i = 0; i < objects.size(); i++) objects[i]->Draw();
	}

	void Move(float dt)
	{
		for (int i = 0; i < objects.size(); i++) objects[i]->Move(dt);
	}

	void Control()
	{
		for (int i = 0; i < objects.size(); i++) objects[i]->Control();

		std::vector<Object*> tmp = objects;
		objects.clear();
		int lifecounter = 0;
		if (newDiamond) {
			objects.push_back(new DiamondCount(textures[3], diamonds));
			newDiamond = false;
		};
		if (mouseClicked && !caught) {
			if (glutGet(GLUT_ELAPSED_TIME) - lastTime > 1000) {
				objects.push_back(new Pokeball(textures[6], lander->GetPosition()));
				lastTime = glutGet(GLUT_ELAPSED_TIME);
			}
		}
		if (mouseClicked && caught) {
			objects.push_back(new FlameThrower(textures[2], lander->GetPosition()));
		}
		for (int i = 0; i < tmp.size(); i++)
		{
			if (tmp[i]->StillAlive())
			{
				if (tmp[i]->GetType() == LIFE) {
					if (lifecounter < lives) {
						objects.push_back(tmp[i]);
						lifecounter += 1;
					}
				}
				else if (tmp[i]->GetType() == LANDER) {
					objects.push_back(tmp[i]);
				}
				else objects.push_back(tmp[i]);
			}
		}
		tmp.clear();
		lifecounter = 0;
	}

	void Interact()
	{
		for (int i = 0; i < objects.size(); i++)
			for (int j = 0; j < objects.size(); j++)
				objects[i]->Interact(objects[j]);
	}
};

Scene scene;

void onInitialization() {
	glViewport(0, 0, windowWidth, windowHeight);
	// Create vertex shader from string
	unsigned int vertexShader0 = glCreateShader(GL_VERTEX_SHADER); // vertex shader 0
	if (!vertexShader0) { printf("Error in vertex shader 0 creation\n"); exit(1); }
	glShaderSource(vertexShader0, 1, &vertexSource0, NULL);
	glCompileShader(vertexShader0);
	checkShader(vertexShader0, "Vertex shader 0 error");

	unsigned int fragmentShader0 = glCreateShader(GL_FRAGMENT_SHADER); // pixel shader 0
	if (!fragmentShader0) { printf("Error in fragment shader 0 creation\n"); exit(1); }
	glShaderSource(fragmentShader0, 1, &fragmentSource0, NULL);
	glCompileShader(fragmentShader0);
	checkShader(fragmentShader0, "Fragment shader 0 error");

	shaderProgram0 = glCreateProgram(); // shaderProgram0 is a global unsigned int 
	if (!shaderProgram0) { printf("Error in shader program 0 creation\n"); exit(1); }
	glAttachShader(shaderProgram0, vertexShader0);
	glAttachShader(shaderProgram0, fragmentShader0);
	glBindAttribLocation(shaderProgram0, 0, "vertexPosition");
	glBindAttribLocation(shaderProgram0, 1, "vertexTexCoord");
	glBindFragDataLocation(shaderProgram0, 0, "fragmentColor");
	glLinkProgram(shaderProgram0);
	checkLinking(shaderProgram0);


	// vertexshader1
	unsigned int vertexShader1 = glCreateShader(GL_VERTEX_SHADER); // vertex shader 1
	if (!vertexShader1) { printf("Error in vertex shader 1 creation\n"); exit(1); }
	glShaderSource(vertexShader1, 1, &vertexSource1, NULL);
	glCompileShader(vertexShader1);
	checkShader(vertexShader1, "Vertex shader error");

	unsigned int fragmentShader1 = glCreateShader(GL_FRAGMENT_SHADER); // pixel shader 1
	if (!fragmentShader1) { printf("Error in fragment shader 1 creation\n"); exit(1); }
	glShaderSource(fragmentShader1, 1, &fragmentSource1, NULL);
	glCompileShader(fragmentShader1);
	checkShader(fragmentShader1, "Fragment shader 1 error");

	shaderProgram1 = glCreateProgram(); // shaderProgram1 is a global unsigned int
	if (!shaderProgram0) { printf("Error in shader program 1 creation\n"); exit(1); }
	glAttachShader(shaderProgram1, vertexShader1);
	glAttachShader(shaderProgram1, fragmentShader1);
	glBindAttribLocation(shaderProgram1, 0, "vertexPosition");
	glBindAttribLocation(shaderProgram1, 1, "vertexColor");
	glBindFragDataLocation(shaderProgram1, 0, "fragmentColor");
	glLinkProgram(shaderProgram1);
	checkLinking(shaderProgram1);

	scene.Initialize();

	for (int i = 0; i < 256; i++) keyPressed[i] = false;
}

void onMouseButton(int button, int state, int x, int y)
{
	if (state == GLUT_DOWN) {
		mouseClicked = true;
		mouseX = x;
		mouseY = y;
	}
	if (state == GLUT_UP) {
		mouseX = x;
		mouseY = y;
		mouseClicked = false;
	}
	glutPostRedisplay();
}

void onKeyboard(unsigned char key, int x, int y)
{
	keyPressed[key] = true;
	keyDown = true;
	glutPostRedisplay();
}

void onKeyboardUp(unsigned char key, int x, int y)
{
	keyPressed[key] = false;
	keyDown = false;
	glutPostRedisplay();
}

void onExit() {
	glDeleteProgram(shaderProgram0);
	printf("exit");
}

void onDisplay() {

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	scene.Draw();

	glutSwapBuffers();
}

void onReshape(int winWidth0, int winHeight0)
{
	glViewport(0, 0, winWidth0, winHeight0);

	windowWidth = winWidth0, windowHeight = winHeight0;
}


void onIdle() {
	double t = glutGet(GLUT_ELAPSED_TIME) * 0.001;
	static double lastTime = 0.0;
	double dt = t - lastTime;
	lastTime = t;

	scene.Interact();
	scene.Control();
	scene.Move(dt);
	scene.Draw();

	glutPostRedisplay();
}


int main(int argc, char * argv[]) {
	glutInit(&argc, argv);
#if !defined(__APPLE__)
	glutInitContextVersion(majorVersion, minorVersion);
#endif
	glutInitWindowSize(windowWidth, windowHeight);
	glutInitWindowPosition(10, 10);
#if defined(__APPLE__)
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_3_2_CORE_PROFILE);
#else
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
#endif
	glutCreateWindow("Game");

#if !defined(__APPLE__)
	glewExperimental = true;
	glewInit();
#endif

	printf("GL Vendor    : %s\n", glGetString(GL_VENDOR));
	printf("GL Renderer  : %s\n", glGetString(GL_RENDERER));
	printf("GL Version (string)  : %s\n", glGetString(GL_VERSION));
	glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
	glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
	printf("GL Version (integer) : %d.%d\n", majorVersion, minorVersion);
	printf("GLSL Version : %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

	onInitialization();

	glutDisplayFunc(onDisplay);
	glutReshapeFunc(onReshape);
	glutKeyboardFunc(onKeyboard);
	glutKeyboardUpFunc(onKeyboardUp);
	glutIdleFunc(onIdle);
	glutMouseFunc(onMouseButton);

	glutMainLoop();
	onExit();
	return 1;
}
