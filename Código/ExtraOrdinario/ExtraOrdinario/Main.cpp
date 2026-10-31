#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//C++ standard libraries
#include <iostream>
#include <fstream>
#include <cmath>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "stb_image.h"
#include "SOIL2/SOIL2.h"
#include "Shader.h"
#include "Camera.h"
#include "Model.h"
#include "Texture.h"
#include "Animation.h"
#include "Animator.h"

// Function prototypes
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode);
void MouseCallback(GLFWwindow* window, double xPos, double yPos);
void DoMovement();
void AnimationKeys();

// Window dimensions
const GLuint WIDTH = 1920, HEIGHT = 1000;
int SCREEN_WIDTH, SCREEN_HEIGHT;

// Camera
Camera  camera(glm::vec3(1.5f, 0.5f,3.0f), glm::vec3(0.0f, 1.0f, 0.0f), 270.0f);
GLfloat lastX = WIDTH / 2.0;
GLfloat lastY = HEIGHT / 2.0;
bool keys[1024];
bool firstMouse = true;

//Luces
glm::vec3 lightPos(0.0f, 0.0f, 0.0f);
bool active;

// Positions of the point lights
glm::vec3 pointLightPositions[] = {
	glm::vec3(0.0f,2.0f, 0.0f)
};

// === Altura del terreno ===
// Triangulos del piso para consultar su altura en (x, z) y asentar los modelos sobre el
vector<glm::vec3> triangulosPiso;

float AlturaTerreno(float x, float z)
{
	for (size_t i = 0; i + 2 < triangulosPiso.size(); i += 3)
	{
		const glm::vec3& a = triangulosPiso[i];
		const glm::vec3& b = triangulosPiso[i + 1];
		const glm::vec3& c = triangulosPiso[i + 2];
		float d = (b.z - c.z) * (a.x - c.x) + (c.x - b.x) * (a.z - c.z);
		if (fabs(d) < 1e-6f) continue;
		float l1 = ((b.z - c.z) * (x - c.x) + (c.x - b.x) * (z - c.z)) / d;
		float l2 = ((c.z - a.z) * (x - c.x) + (a.x - c.x) * (z - c.z)) / d;
		float l3 = 1.0f - l1 - l2;
		if (l1 >= -1e-4f && l2 >= -1e-4f && l3 >= -1e-4f)
			return l1 * a.y + l2 * b.y + l3 * c.y;
	}
	return 0.0f;
}

// Escala para que la dimension mas grande del modelo mida "tamano" unidades de la escena
float EscalaModelo(const Model& m, float tamano)
{
	glm::vec3 dim = m.maxBounds - m.minBounds;
	float mayor = glm::max(dim.x, glm::max(dim.y, dim.z));
	return mayor > 0.0f ? tamano / mayor : 1.0f;
}

// Matriz para un modelo descargado: lo escala, lo centra y apoya su base en el terreno (mas una elevacion opcional)
glm::mat4 ColocarModelo(const Model& m, float x, float z, float rotY, float tamano, float elevacion = 0.0f)
{
	glm::vec3 centro = (m.minBounds + m.maxBounds) * 0.5f;
	glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(x, AlturaTerreno(x, z) + elevacion, z));
	t = glm::rotate(t, glm::radians(rotY), glm::vec3(0.0f, 1.0f, 0.0f));
	t = glm::scale(t, glm::vec3(EscalaModelo(m, tamano)));
	t = glm::translate(t, glm::vec3(-centro.x, -m.minBounds.y, -centro.z));
	return t;
}

// Caja envolvente de un modelo con huesos en su primera pose: el esqueleto puede escalar/rotar la malla
// (p. ej. Mixamo trabaja en centimetros), asi que se aplica el skinning en CPU para medirlo bien
void MedirConHuesos(Model& m, Animator& a)
{
	a.Reset();
	const auto& huesos = a.GetFinalBoneMatrices();
	m.minBounds = glm::vec3(1e9f);
	m.maxBounds = glm::vec3(-1e9f);
	for (const Mesh& mesh : m.GetMeshes())
		for (const Vertex& v : mesh.vertices)
		{
			glm::vec4 p(0.0f);
			for (int k = 0; k < MAX_BONE_INFLUENCE; k++)
				if (v.m_BoneIDs[k] >= 0 && v.m_BoneIDs[k] < (int)huesos.size())
					p += huesos[v.m_BoneIDs[k]] * glm::vec4(v.Position, 1.0f) * v.m_Weights[k];
			if (p.w == 0.0f) p = glm::vec4(v.Position, 1.0f);
			m.minBounds = glm::min(m.minBounds, glm::vec3(p));
			m.maxBounds = glm::max(m.maxBounds, glm::vec3(p));
		}
}

// === Modelos estaticos de la Revolucion ===
const char* rutasEstaticos[] = {
	"ModelsExtra/Convertidos/Barril/Barril.obj",                                             // 0
	"ModelsExtra/ModelosPolyPizza/Cathedral by Bruno Oliveira - fEJKTKNRAsN.glb",            // 1
	"ModelsExtra/ModelosPolyPizza/Curved road by Poly by Google - 9r2Owu4oVHw.glb",          // 2
	"ModelsExtra/ModelosPolyPizza/Horse by Quaternius - qvTrSG9pZF/Horse.fbx",               // 3
	"ModelsExtra/Convertidos/Casa/Casa.obj",                                                 // 4
	"ModelsExtra/Convertidos/Nopal/Nopal.obj",                                               // 5
	"ModelsExtra/Convertidos/Rifle/Rifle.obj",                                               // 6
	"ModelsExtra/ModelosPolyPizza/Sombrero by Poly by Google - 7fGDqHvHap1/Sombrero.obj"     // 7
};
#define NUM_MODELOS_ESTATICOS 8

struct ObjetoEscena {
	int modelo;        // indice en rutasEstaticos
	float x, z;
	float rotY;        // grados
	float tamano;      // dimension mas grande, en unidades de la escena
	float elevacion;   // sobre el terreno
};

ObjetoEscena objetosEscena[] = {
	{ 1,   2.0f, -26.0f,   0.0f, 9.0f, 0.0f },    // catedral
	{ 4,  11.0f, -18.0f, -90.0f, 4.0f, 0.0f },    // casa
	{ 4,  -1.5f, -19.5f,  90.0f, 4.0f, 0.0f },    // casa
	{ 3,   8.0f, -13.0f,  30.0f, 1.6f, 0.0f },    // caballo
	{ 2,  16.0f,  -6.0f,   0.0f, 6.0f, 0.0f },    // camino de tierra
	{ 0,   6.5f, -16.0f,   0.0f, 0.5f, 0.0f },    // barriles
	{ 0,   7.1f, -16.3f,   0.0f, 0.5f, 0.0f },
	{ 0,   6.8f, -15.5f,   0.0f, 0.5f, 0.0f },
	{ 7,   6.5f, -16.0f,  20.0f, 0.45f, 0.5f },   // sombrero sobre un barril
	{ 6,   5.8f, -15.6f,  70.0f, 0.9f, 0.0f },    // rifle junto a los barriles
	{ 5,   6.0f,   4.0f,   0.0f, 0.9f, 0.0f },    // nopales
	{ 5,  14.0f, -14.0f,  45.0f, 1.0f, 0.0f },
	{ 5,  -8.0f,  -4.0f,  90.0f, 0.8f, 0.0f },
	{ 5,  20.0f,  -8.0f, 135.0f, 1.1f, 0.0f }
};
#define NUM_OBJETOS_ESCENA (sizeof(objetosEscena) / sizeof(objetosEscena[0]))

// === Modelos animados ===
// Bucle 1: tren que va y viene por la via (z = -1)
float tiempoTren = 0.0f;
const float trenCentroX = 29.0f, trenAmplitud = 12.0f, trenZ = -1.0f;
const float trenRot = 0.0f;   // los modelos ya vienen a lo largo del eje X
// Bucle 2: campana oscilando; C pausa / reanuda
float tiempoCampana = 0.0f;
bool campanaPausada = false;
const glm::vec3 campanarioPos = glm::vec3(8.0f, 0.0f, -24.0f);
// Bucle 3: canon con retroceso al disparar
float tiempoCanon = 0.0f;
// Procedural: bandera ondeando (desplazamiento de vertices en el shader)
float tiempoBandera = 0.0f;
// Medidas de la tela roja en coordenadas del modelo (Bandera.obj): X de -0.05 a 0.86, Y de 1.57 a 2.07
const float banderaAsta = -0.05f;      // X donde la tela se une al asta
const float banderaLargo = 0.91f;      // largo de la tela en X
const float banderaAlturaMin = 1.5f;   // debajo de esta altura (asta y base) no hay onda
// Tamano de los personajes con huesos (dimension mas grande de su malla)
float escalaJefe = 1.1f;
float escalaAdelita = 1.0f;
// Steve (personaje de referencia del proyecto base) no pertenece a la tematica
bool mostrarSteve = false;

// === Arboles ===
// y = altura minima del terreno bajo el tronco (calculada de Floor.obj), para que no floten ni se hundan
#define NUM_ARBOLES 9
glm::vec3 posArboles[NUM_ARBOLES] = {
	// Arboles originales (solo se corrigio la altura)
	glm::vec3(0.0f, 0.347f, 8.0f),
	glm::vec3(8.0f, 0.0f, 0.0f),
	glm::vec3(-8.0f, 0.169f, 0.0f),
	glm::vec3(0.0f, 0.0f, -10.0f),
	glm::vec3(3.0f, 0.0f, -15.0f),
	// Arboles agregados
	glm::vec3(-6.0f, 0.0f, -8.0f),
	glm::vec3(10.0f, 0.0f, -9.0f),
	glm::vec3(12.0f, 0.0f, 6.0f),
	glm::vec3(-10.0f, 0.167f, -14.0f)
};

#define MAX_FRAMES 20
typedef struct _frame {
	float rotGato;
	float rotGatoInc;
	float gatoPosX;
	float gatoPosY;
	float gatoPosZ;
	float incX;
	float incY;
	float incZ;
	float headGato;
	float headIncGato;
	float FLegsGato;
	float FLegsIncGato;
	float BLegsGato;
	float BLegsIncGato;
	float tailGato;
	float tailIncGato;
	float inclinacionGato;
	float inclinacionIncGato;
} FRAME;

FRAME KeyFrame[MAX_FRAMES];
int FrameIndex = 0;
bool play = false;
int playIndex = 0;

float vertices[] = {
	 -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
	   -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
	   -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

	   -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
	   -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
	   -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

	   -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
	   -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
	   -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
	   -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
	   -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
	   -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

		0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
		0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
		0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

	   -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
		0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
		0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
		0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
	   -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
	   -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

	   -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
		0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
		0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
		0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
	   -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
	   -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
};

// === Animacion ciclo dia-noche ===
float sunAngle = 0.0f;
float sunSpeed = 0.2f;
float sunRadius = 10.0f;
bool dayNightCycle = true;
glm::vec3 sunPosition(0.0f);

// Intensidad luz puntual, se cambia con SPACE
glm::vec3 Light1 = glm::vec3(0);

// === Luz direccional ===
// Blanco tenue por defecto; la tecla L alterna entre blanco y naranja
bool luzNaranja = false;
const float intensidadDir = 0.5f;
const glm::vec3 colorBlanco = glm::vec3(1.0f, 1.0f, 1.0f);
const glm::vec3 colorNaranja = glm::vec3(1.0f, 0.5f, 0.0f);

// === Spotlights RGB ===
// Iluminan al personaje principal desde 3 lados; R, V y B encienden/apagan cada una
#define NUM_SPOTLIGHTS 3
const glm::vec3 spotObjetivo = glm::vec3(4.5f, 0.8f, -9.0f);   // personaje iluminado
const glm::vec3 spotPos[NUM_SPOTLIGHTS] = {
	glm::vec3(4.5f, 3.0f, -7.0f),     // roja: al frente
	glm::vec3(2.77f, 3.0f, -10.0f),   // verde: atras a la izquierda
	glm::vec3(6.23f, 3.0f, -10.0f)    // azul: atras a la derecha
};
const glm::vec3 spotColor[NUM_SPOTLIGHTS] = {
	glm::vec3(1.0f, 0.0f, 0.0f),
	glm::vec3(0.0f, 1.0f, 0.0f),
	glm::vec3(0.0f, 0.0f, 1.0f)
};
bool spotEncendida[NUM_SPOTLIGHTS] = { true, true, true };

// === Zorro (animacion por codigo) ===
// Z: inicia/detiene la caminata (ida y vuelta por un carril libre), X: salto
bool zorroCaminando = false;
bool zorroSaltando = false;
float zorroPosX = -3.0f;
const float zorroPosZ = -4.0f;
const float zorroPisoY = 0.0f;          // altura del piso en el carril (zona plana)
const float zorroLimiteMinX = -3.0f;
const float zorroLimiteMaxX = 5.0f;
float zorroDir = 1.0f;                  // 1 = hacia +X, -1 = hacia -X
float zorroRot = 90.0f;                 // orientacion en Y; el modelo mira a +Z, 90 = mirando a +X
const float zorroEscala = 0.2f;
const float zorroVelocidad = 1.0f;      // unidades por segundo
const float zorroVelGiro = 180.0f;      // grados por segundo al dar la vuelta
const float zorroFrecPaso = 10.0f;      // radianes por segundo del ciclo de pasos
float zorroCiclo = 0.0f;                // fase del ciclo de pasos
float zorroAmp = 0.0f;                  // amplitud del paso (0 = quieto, 1 = caminando)
float zorroSaltoT = 0.0f;               // progreso del salto [0,1]
const float zorroAlturaSalto = 0.5f;
const float zorroDuracionSalto = 0.9f;  // segundos


// === Animaciones FBX (por esqueleto) ===
bool playFBX = false;
Animation* g_SteveAnim = nullptr;
Animator* g_SteveAnimator = nullptr;
float steveSpeed = 1.0f;
int steveEstado = 0;
float steveDir = 1.0f;
float stevePosX = 0.0f, stevePosY = 0.0f, stevePosZ = 0.0f;




#define MAX_FRAMES 16
int i_max_steps = 140;
int i_curr_steps = 0;


// Deltatime
GLfloat deltaTime = 0.0f;	// Time between current frame and last frame
GLfloat lastFrame = 0.0f;  	// Time of last frame

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);   // la consola muestra los mensajes al instante

	// Solo para pruebas: EXTRA_CAMARA="x,y,z,yaw,pitch" coloca la camara en otra posicion inicial
	char* camaraEnv = nullptr;
	size_t camaraLen = 0;
	if (_dupenv_s(&camaraEnv, &camaraLen, "EXTRA_CAMARA") == 0 && camaraEnv)
	{
		float cx, cy, cz, cyaw, cpitch;
		if (sscanf_s(camaraEnv, "%f,%f,%f,%f,%f", &cx, &cy, &cz, &cyaw, &cpitch) == 5)
			camera = Camera(glm::vec3(cx, cy, cz), glm::vec3(0.0f, 1.0f, 0.0f), cyaw, cpitch);
		free(camaraEnv);
	}

	// Inicializacion de GLFW y creacion de ventana
	glfwInit();
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Extraordinario CG", nullptr, nullptr);

	if (nullptr == window)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();

		return EXIT_FAILURE;
	}

	glfwMakeContextCurrent(window);

	glfwGetFramebufferSize(window, &SCREEN_WIDTH, &SCREEN_HEIGHT);

	// Set the required callback functions
	glfwSetKeyCallback(window, KeyCallback);
	glfwSetCursorPosCallback(window, MouseCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// Set this to true so GLEW knows to use a modern approach to retrieving function pointers and extensions
	glewExperimental = GL_TRUE;
	// Initialize GLEW to setup the OpenGL Function pointers
	if (GLEW_OK != glewInit())
	{
		std::cout << "Failed to initialize GLEW" << std::endl;
		return EXIT_FAILURE;
	}

	// Define the viewport dimensions
	glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

	// === Carga de shaders ===
	Shader lightingShader("Shader/lighting.vs", "Shader/lighting.frag");
	Shader lampShader("Shader/lamp.vs", "Shader/lamp.frag");
	Shader skyboxShader("Shader/SkyBox.vs", "Shader/SkyBox.frag");


	// === Carga de modelos ===

	//Model Aldea
	Model Aldea((char*)"ModelsExtra/Floor.obj");
	for (const Mesh& m : Aldea.GetMeshes())
		for (size_t i = 0; i + 2 < m.indices.size(); i += 3)
			for (int k = 0; k < 3; k++)
				triangulosPiso.push_back(m.vertices[m.indices[i + k]].Position);


	Model tree((char*)"ModelsExtra/Tree/Tree.obj");

	//Model Zorro (por partes para poder animarlo)
	Model zorroCuerpo((char*)"ModelsExtra/Zorro/cuerpo.obj");
	Model zorroCabeza((char*)"ModelsExtra/Zorro/cabeza.obj");
	Model zorroCola((char*)"ModelsExtra/Zorro/cola.obj");
	Model zorroPataDerDelante((char*)"ModelsExtra/Zorro/pataDerDelante.obj");
	Model zorroPataIzqDelante((char*)"ModelsExtra/Zorro/pataIzqDelante.obj");
	Model zorroPataDerDetras((char*)"ModelsExtra/Zorro/pataDerDetras.obj");
	Model zorroPataIzqDetras((char*)"ModelsExtra/Zorro/pataIzqDetras.obj");

	// Modelos de la Revolucion (estaticos: se aplican las transformaciones de sus nodos)
	vector<Model*> modelosEstaticos;
	for (int i = 0; i < NUM_MODELOS_ESTATICOS; i++)
		modelosEstaticos.push_back(new Model((char*)rutasEstaticos[i], true));
	Model locomotora((char*)"ModelsExtra/Convertidos/Locomotora/Locomotora.obj", true);
	Model vagon((char*)"ModelsExtra/Convertidos/Vagon/Vagon.obj", true);
	Model campana((char*)"ModelsExtra/Convertidos/Campana/Campana.obj", true);
	Model canon((char*)"ModelsExtra/Convertidos/Canon/Canon.obj", true);
	Model bandera((char*)"ModelsExtra/Convertidos/Bandera/Bandera.obj", true);

	// Personajes con huesos (Mixamo): animacion por esqueleto en bucle
	Model jefe((char*)"ModelsExtra/ModelosPolyPizza/The boss/Reloading.dae");
	Animation animJefe("ModelsExtra/ModelosPolyPizza/The boss/Reloading.dae", &jefe);
	Animator animadorJefe(&animJefe);
	Model adelita((char*)"ModelsExtra/ModelosPolyPizza/peasant girl/Excited.dae");
	Animation animAdelita("ModelsExtra/ModelosPolyPizza/peasant girl/Excited.dae", &adelita);
	Animator animadorAdelita(&animAdelita);
	MedirConHuesos(jefe, animadorJefe);
	MedirConHuesos(adelita, animadorAdelita);

	// Diagnostico: tamano de cada modelo al cargarlo
	auto reportar = [](const char* nombre, const Model& m) {
		printf("%-12s min(%.2f, %.2f, %.2f) max(%.2f, %.2f, %.2f) meshes=%d\n", nombre,
			m.minBounds.x, m.minBounds.y, m.minBounds.z, m.maxBounds.x, m.maxBounds.y, m.maxBounds.z, (int)m.GetMeshes().size());
	};
	for (int i = 0; i < NUM_MODELOS_ESTATICOS; i++) reportar(rutasEstaticos[i] + 12, *modelosEstaticos[i]);
	reportar("locomotora", locomotora); reportar("vagon", vagon); reportar("campana", campana);
	reportar("canon", canon); reportar("bandera", bandera); reportar("jefe", jefe); reportar("adelita", adelita);

	//Model Steve
	Model Steve((char*)"ModelsExtra/Steve/steveAnimated.fbx");
	g_SteveAnim = new Animation("ModelsExtra/Steve/steveAnimated.fbx", &Steve);
	g_SteveAnimator = new Animator(g_SteveAnim);
	g_SteveAnimator->Reset();



	// Vertices skybox
	GLfloat skyboxVertices[] = {
		// Positions
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		1.0f,  1.0f, -1.0f,
		1.0f,  1.0f,  1.0f,
		1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		1.0f, -1.0f,  1.0f
	};

	GLuint indices[] =
	{  // Note that we start from 0!
		0,1,2,3,
		4,5,6,7,
		8,9,10,11,
		12,13,14,15,
		16,17,18,19,
		20,21,22,23,
		24,25,26,27,
		28,29,30,31,
		32,33,34,35
	};

	// First, set the container's VAO (and VBO)
	GLuint VBO, VAO, EBO;
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);


	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);


	// Position attribute
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
	glEnableVertexAttribArray(0);
	// normal attribute
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	// Set texture units
	lightingShader.Use();
	glUniform1i(glGetUniformLocation(lightingShader.Program, "Material.difuse"), 0);
	glUniform1i(glGetUniformLocation(lightingShader.Program, "Material.specular"), 1);

	//Skybox
	GLuint skyboxVBO, skyboxVAO;
	glGenVertexArrays(1, &skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindVertexArray(skyboxVAO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);

	// Load textures
	vector < const GLchar*> faces;
	faces.push_back("SkyBox/right.jpg");
	faces.push_back("SkyBox/left.jpg");
	faces.push_back("SkyBox/top.jpg");
	faces.push_back("SkyBox/bottom.jpg");
	faces.push_back("SkyBox/back.jpg");
	faces.push_back("SkyBox/front.jpg");

	GLuint cubemapTexture = TextureLoading::LoadCubemap(faces);


	glm::mat4 projection = glm::perspective(camera.GetZoom(), (GLfloat)SCREEN_WIDTH / (GLfloat)SCREEN_HEIGHT, 0.1f, 100.0f);

	// === Loop para renderizar la escena ===
	while (!glfwWindowShouldClose(window))
	{

		// Calculate deltatime of current frame
		GLfloat currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		sunPosition = glm::vec3(0.0f, 10.0f, 0.0f);

		// Dirección de la luz: hacia abajo, ligeramente inclinada para que las caras laterales tengan sombreado
		glm::vec3 sunDir = glm::vec3(-0.3f, -1.0f, -0.4f);

		// Check if any events have been activiated (key pressed, mouse moved etc.) and call corresponding response functions
		glfwPollEvents();
		DoMovement();
		AnimationKeys();


		// Clear the colorbuffer

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// OpenGL options
		glEnable(GL_DEPTH_TEST);


		glm::mat4 modelTemp = glm::mat4(1.0f); //Temp

		
		// Use corresponding shader when setting uniforms/drawing objects
		lightingShader.Use();


		glUniform1i(glGetUniformLocation(lightingShader.Program, "diffuse"), 0);
		//glUniform1i(glGetUniformLocation(lightingShader.Program, "specular"),1);

		GLint viewPosLoc = glGetUniformLocation(lightingShader.Program, "viewPos");
		glUniform3f(viewPosLoc, camera.GetPosition().x, camera.GetPosition().y, camera.GetPosition().z);

		// === Directional light ===
		// Color blanco o naranja (tecla L), ambos con intensidad 0.5
		glm::vec3 dirColor = luzNaranja ? colorNaranja : colorBlanco;
		glm::vec3 ambient = dirColor * intensidadDir * 0.6f;
		glm::vec3 diffuse = dirColor * intensidadDir;
		glm::vec3 specular = dirColor * intensidadDir * 0.3f;

		glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.direction"), sunDir.x, sunDir.y, sunDir.z);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.ambient"), ambient.x, ambient.y, ambient.z);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.diffuse"), diffuse.x, diffuse.y, diffuse.z);
		glUniform3f(glGetUniformLocation(lightingShader.Program, "dirLight.specular"), specular.x, specular.y, specular.z);

		// === Spotlights RGB (una apagada envia color negro) ===
		for (int i = 0; i < NUM_SPOTLIGHTS; i++)
		{
			string base = "spotLights[" + to_string(i) + "].";
			glm::vec3 dir = glm::normalize(spotObjetivo - spotPos[i]);
			glm::vec3 col = spotEncendida[i] ? spotColor[i] * 2.0f : glm::vec3(0.0f);
			glUniform3f(glGetUniformLocation(lightingShader.Program, (base + "position").c_str()), spotPos[i].x, spotPos[i].y, spotPos[i].z);
			glUniform3f(glGetUniformLocation(lightingShader.Program, (base + "direction").c_str()), dir.x, dir.y, dir.z);
			glUniform1f(glGetUniformLocation(lightingShader.Program, (base + "cutOff").c_str()), glm::cos(glm::radians(15.0f)));
			glUniform1f(glGetUniformLocation(lightingShader.Program, (base + "outerCutOff").c_str()), glm::cos(glm::radians(22.0f)));
			glUniform1f(glGetUniformLocation(lightingShader.Program, (base + "constant").c_str()), 1.0f);
			glUniform1f(glGetUniformLocation(lightingShader.Program, (base + "linear").c_str()), 0.09f);
			glUniform1f(glGetUniformLocation(lightingShader.Program, (base + "quadratic").c_str()), 0.032f);
			glUniform3f(glGetUniformLocation(lightingShader.Program, (base + "ambient").c_str()), 0.0f, 0.0f, 0.0f);
			glUniform3f(glGetUniformLocation(lightingShader.Program, (base + "diffuse").c_str()), col.x, col.y, col.z);
			glUniform3f(glGetUniformLocation(lightingShader.Program, (base + "specular").c_str()), col.x, col.y, col.z);
		}


		// Set material properties
		glUniform1f(glGetUniformLocation(lightingShader.Program, "material.shininess"), 5.0f);


		// Create camera transformations
		glm::mat4 view;
		view = camera.GetViewMatrix();

		// Get the uniform locations
		GLint modelLoc = glGetUniformLocation(lightingShader.Program, "model");
		GLint viewLoc = glGetUniformLocation(lightingShader.Program, "view");
		GLint projLoc = glGetUniformLocation(lightingShader.Program, "projection");

		// Pass the matrices to the shader
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));


		glm::mat4 model(1);
		glUniform1i(glGetUniformLocation(lightingShader.Program, "useBones"), 0);

		view = camera.GetViewMatrix();
		model = glm::mat4(1);
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		Aldea.Draw(lightingShader);

		// === Arboles ===
		for (int i = 0; i < NUM_ARBOLES; i++)
		{
			model = glm::mat4(1);
			model = glm::translate(model, posArboles[i]);
			model = glm::scale(model, glm::vec3(2.0f));
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
			tree.Draw(lightingShader);
		}

		// === Zorro ===
		// Pose de caminata: trote con pares diagonales de patas, cola y cabeza oscilando
		float paso = sin(zorroCiclo);
		float anguloPatas = 30.0f * zorroAmp * paso;
		float anguloCola = 15.0f * zorroAmp * paso;
		float anguloCabeza = 4.0f * zorroAmp * sin(2.0f * zorroCiclo);
		float rebote = 0.015f * zorroAmp * fabs(paso);

		// Pose de salto: arco parabolico, patas estiradas, cuerpo inclinado (nariz arriba al despegar, abajo al caer)
		float saltoY = 0.0f, inclinacion = 0.0f, colaSalto = 0.0f;
		float patasDelSalto = 0.0f, patasTrasSalto = 0.0f;
		if (zorroSaltando)
		{
			float t = zorroSaltoT;
			float arco = sin(glm::pi<float>() * t);
			saltoY = 4.0f * zorroAlturaSalto * t * (1.0f - t);
			inclinacion = -20.0f * sin(2.0f * glm::pi<float>() * t);
			patasDelSalto = -45.0f * arco;
			patasTrasSalto = 45.0f * arco;
			colaSalto = 25.0f * arco;
		}

		// Transformacion base: las partes vienen en su posicion final, se escala y se centra el cuerpo en el origen
		glm::mat4 zorroBase = glm::mat4(1.0f);
		zorroBase = glm::translate(zorroBase, glm::vec3(zorroPosX, zorroPisoY + saltoY + rebote, zorroPosZ));
		zorroBase = glm::rotate(zorroBase, glm::radians(zorroRot), glm::vec3(0.0f, 1.0f, 0.0f));
		zorroBase = glm::rotate(zorroBase, glm::radians(inclinacion), glm::vec3(1.0f, 0.0f, 0.0f));
		zorroBase = glm::scale(zorroBase, glm::vec3(zorroEscala));
		zorroBase = glm::translate(zorroBase, glm::vec3(0.0f, 1.21f, 1.23f));

		// Dibuja una parte rotandola sobre su pivote (en coordenadas del modelo)
		auto dibujarParteZorro = [&](Model& parte, glm::vec3 pivote, glm::mat4 rotacion)
		{
			glm::mat4 m = zorroBase;
			m = glm::translate(m, pivote);
			m = m * rotacion;
			m = glm::translate(m, -pivote);
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(m));
			parte.Draw(lightingShader);
		};
		const glm::vec3 ejeX = glm::vec3(1.0f, 0.0f, 0.0f);
		const glm::vec3 ejeY = glm::vec3(0.0f, 1.0f, 0.0f);
		float patasA = zorroSaltando ? patasDelSalto : anguloPatas;    // delantera izq.
		float patasB = zorroSaltando ? patasDelSalto : -anguloPatas;   // delantera der.
		float patasC = zorroSaltando ? patasTrasSalto : -anguloPatas;  // trasera izq.
		float patasD = zorroSaltando ? patasTrasSalto : anguloPatas;   // trasera der.

		dibujarParteZorro(zorroCuerpo, glm::vec3(0.0f), glm::mat4(1.0f));
		dibujarParteZorro(zorroCabeza, glm::vec3(0.0f, 0.0f, -0.67f),
			glm::rotate(glm::mat4(1.0f), glm::radians(anguloCabeza), ejeX));
		dibujarParteZorro(zorroCola, glm::vec3(0.0f, -0.05f, -1.79f),
			glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(anguloCola), ejeY), glm::radians(colaSalto), ejeX));
		dibujarParteZorro(zorroPataIzqDelante, glm::vec3(0.0f, -0.49f, -0.915f),
			glm::rotate(glm::mat4(1.0f), glm::radians(patasA), ejeX));
		dibujarParteZorro(zorroPataDerDelante, glm::vec3(0.0f, -0.49f, -0.915f),
			glm::rotate(glm::mat4(1.0f), glm::radians(patasB), ejeX));
		dibujarParteZorro(zorroPataIzqDetras, glm::vec3(0.0f, -0.49f, -1.515f),
			glm::rotate(glm::mat4(1.0f), glm::radians(patasC), ejeX));
		dibujarParteZorro(zorroPataDerDetras, glm::vec3(0.0f, -0.49f, -1.515f),
			glm::rotate(glm::mat4(1.0f), glm::radians(patasD), ejeX));

		// === Modelos estaticos de la Revolucion ===
		for (size_t i = 0; i < NUM_OBJETOS_ESCENA; i++)
		{
			const ObjetoEscena& o = objetosEscena[i];
			model = ColocarModelo(*modelosEstaticos[o.modelo], o.x, o.z, o.rotY, o.tamano, o.elevacion);
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
			modelosEstaticos[o.modelo]->Draw(lightingShader);
		}

		// Cajas de color (via del tren y campanario) usando el cubo del proyecto
		auto dibujarCaja = [&](glm::vec3 centro, glm::vec3 tam, glm::vec3 color)
		{
			glm::mat4 m = glm::translate(glm::mat4(1.0f), centro);
			m = glm::scale(m, tam);
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(m));
			glUniform1i(glGetUniformLocation(lightingShader.Program, "usarTextura"), 0);
			glUniform3f(glGetUniformLocation(lightingShader.Program, "colorDifuso"), color.x, color.y, color.z);
			glBindVertexArray(VAO);
			glDrawArrays(GL_TRIANGLES, 0, 36);
			glBindVertexArray(0);
		};

		// === Bucle 1: tren que va y viene por la via ===
		const glm::vec3 colorDurmiente(0.35f, 0.22f, 0.12f), colorRiel(0.25f, 0.25f, 0.28f);
		for (float x = 12.0f; x <= 48.0f; x += 1.0f)
			dibujarCaja(glm::vec3(x, 0.02f, trenZ), glm::vec3(0.18f, 0.04f, 1.1f), colorDurmiente);
		dibujarCaja(glm::vec3(30.0f, 0.06f, trenZ - 0.35f), glm::vec3(36.0f, 0.05f, 0.06f), colorRiel);
		dibujarCaja(glm::vec3(30.0f, 0.06f, trenZ + 0.35f), glm::vec3(36.0f, 0.05f, 0.06f), colorRiel);
		// Movimiento senoidal: frena suavemente en cada extremo antes de regresar
		float trenX = trenCentroX + trenAmplitud * sin(0.3f * tiempoTren);
		model = ColocarModelo(locomotora, trenX, trenZ, trenRot, 3.5f, 0.08f);
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		locomotora.Draw(lightingShader);
		model = ColocarModelo(vagon, trenX + 3.7f, trenZ, trenRot, 3.5f, 0.08f);   // el frente de la locomotora mira a -X
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		vagon.Draw(lightingShader);

		// === Bucle 2: campana oscilando en su campanario (C pausa / reanuda) ===
		float yCampanario = AlturaTerreno(campanarioPos.x, campanarioPos.z);
		const glm::vec3 colorMadera(0.45f, 0.30f, 0.16f);
		dibujarCaja(campanarioPos + glm::vec3(-0.6f, yCampanario + 1.1f, 0.0f), glm::vec3(0.12f, 2.2f, 0.12f), colorMadera);
		dibujarCaja(campanarioPos + glm::vec3(0.6f, yCampanario + 1.1f, 0.0f), glm::vec3(0.12f, 2.2f, 0.12f), colorMadera);
		dibujarCaja(campanarioPos + glm::vec3(0.0f, yCampanario + 2.2f, 0.0f), glm::vec3(1.4f, 0.12f, 0.12f), colorMadera);
		float anguloCampana = 30.0f * sin(glm::pi<float>() * tiempoCampana);   // periodo de 2 s
		glm::vec3 centroCampana = (campana.minBounds + campana.maxBounds) * 0.5f;
		model = glm::translate(glm::mat4(1.0f), campanarioPos + glm::vec3(0.0f, yCampanario + 2.14f, 0.0f));
		model = glm::rotate(model, glm::radians(anguloCampana), glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::scale(model, glm::vec3(EscalaModelo(campana, 0.6f)));
		model = glm::translate(model, glm::vec3(-centroCampana.x, -campana.maxBounds.y, -centroCampana.z));  // cuelga de su parte superior
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		campana.Draw(lightingShader);

		// === Bucle 3: canon con retroceso (dispara cada 3 s) ===
		float faseCanon = fmod(tiempoCanon, 3.0f);
		float retroceso = faseCanon < 0.1f ? 0.35f * (faseCanon / 0.1f)
			: 0.35f * glm::max(0.0f, 1.0f - (faseCanon - 0.1f) / 1.2f);
		const float canonRot = 0.0f;
		glm::vec3 frenteCanon(sin(glm::radians(canonRot)), 0.0f, cos(glm::radians(canonRot)));
		model = ColocarModelo(canon, -4.0f - frenteCanon.x * retroceso, -13.0f - frenteCanon.z * retroceso, canonRot, 1.2f);
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		canon.Draw(lightingShader);

		// === Procedural: bandera ondeando (el vertex shader desplaza la tela con ondas senoidales) ===
		glUniform1i(glGetUniformLocation(lightingShader.Program, "ondear"), 1);
		glUniform1f(glGetUniformLocation(lightingShader.Program, "tiempo"), tiempoBandera);
		glUniform1f(glGetUniformLocation(lightingShader.Program, "ondaInicio"), banderaAsta);
		glUniform1f(glGetUniformLocation(lightingShader.Program, "ondaLargo"), banderaLargo);
		glUniform1f(glGetUniformLocation(lightingShader.Program, "ondaAlturaMin"), banderaAlturaMin);
		model = ColocarModelo(bandera, 1.0f, -14.0f, 0.0f, 2.2f);
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		bandera.Draw(lightingShader);
		glUniform1i(glGetUniformLocation(lightingShader.Program, "ondear"), 0);

		// === Personajes con huesos (Mixamo): animacion por esqueleto en bucle ===
		animadorJefe.UpdateAnimation(deltaTime);
		animadorAdelita.UpdateAnimation(deltaTime);
		auto dibujarConHuesos = [&](Model& m, Animator& a, glm::mat4 mat)
		{
			glUniform1i(glGetUniformLocation(lightingShader.Program, "useBones"), 1);
			const auto& huesos = a.GetFinalBoneMatrices();
			for (int i = 0; i < (int)huesos.size(); i++)
			{
				string nombre = "finalBonesMatrices[" + to_string(i) + "]";
				glUniformMatrix4fv(glGetUniformLocation(lightingShader.Program, nombre.c_str()), 1, GL_FALSE, glm::value_ptr(huesos[i]));
			}
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(mat));
			m.Draw(lightingShader);
			glUniform1i(glGetUniformLocation(lightingShader.Program, "useBones"), 0);
		};
		dibujarConHuesos(jefe, animadorJefe, ColocarModelo(jefe, spotObjetivo.x, spotObjetivo.z, 0.0f, escalaJefe));
		dibujarConHuesos(adelita, animadorAdelita, ColocarModelo(adelita, -1.0f, -7.0f, 20.0f, escalaAdelita));


		// === Animacion de Steve caminando ===
		model = modelTemp;
		if (playFBX)
		{
			// Maquina estados
			if (steveEstado == 0) // Estado 0: Caminando hacia adelante
			{
				g_SteveAnimator->UpdateAnimation(deltaTime);
				stevePosZ += steveDir * steveSpeed * deltaTime;

				if (stevePosZ >= 2.8f)
				{
					stevePosZ = 2.8f;   
					steveDir = -1.0f;
					steveEstado = 1;    
				}
			}
			else if (steveEstado == 1) // Estado 1: Regresando al origen
			{
				g_SteveAnimator->UpdateAnimation(deltaTime);
				stevePosZ += steveDir * steveSpeed * deltaTime;

				if (stevePosZ <= 0.0f)
				{
					stevePosZ = 0.0f;    
					steveEstado = 2;     
					playFBX = false;
				}
			}

			glUniform1i(glGetUniformLocation(lightingShader.Program, "useBones"), 1);
			model = glm::translate(model, glm::vec3(2.0f + stevePosX, 0.1f + stevePosY, 0.0f + stevePosZ));
			if (steveEstado == 1)
				model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			model = glm::scale(model, glm::vec3(0.05f));
			const auto& transforms = g_SteveAnimator->GetFinalBoneMatrices();
			for (int i = 0; i < (int)transforms.size(); i++)
			{
				string uniformName = "finalBonesMatrices[" + to_string(i) + "]";
				glUniformMatrix4fv(
					glGetUniformLocation(lightingShader.Program, uniformName.c_str()),
					1, GL_FALSE, glm::value_ptr(transforms[i])
				);
			}
		}
		else
		{
			glUniform1i(glGetUniformLocation(lightingShader.Program, "useBones"), 0);
			model = glm::translate(model, glm::vec3(2.0f + stevePosX, 0.1f + stevePosY, 0.0f + stevePosZ));
			model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
			model = glm::scale(model, glm::vec3(0.05f));
		}
		glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
		if (mostrarSteve)
			Steve.Draw(lightingShader);

		glUniform1i(glGetUniformLocation(lightingShader.Program, "useBones"), 0);


		// Also draw the lamp object, again binding the appropriate shader
		lampShader.Use();
		// Get location objects for the matrices on the lamp shader (these could be different on a different shader)
		modelLoc = glGetUniformLocation(lampShader.Program, "model");
		viewLoc = glGetUniformLocation(lampShader.Program, "view");
		projLoc = glGetUniformLocation(lampShader.Program, "projection");

		// Set matrices
		glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

		// Cubos que marcan cada spotlight (de su color si esta encendida, gris si esta apagada)
		glBindVertexArray(VAO);
		for (int i = 0; i < NUM_SPOTLIGHTS; i++)
		{
			model = glm::mat4(1);
			model = glm::translate(model, spotPos[i]);
			model = glm::scale(model, glm::vec3(0.2f));
			glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
			glm::vec3 c = spotEncendida[i] ? spotColor[i] : glm::vec3(0.3f);
			glUniform3f(glGetUniformLocation(lampShader.Program, "lampColor"), c.x, c.y, c.z);
			glDrawArrays(GL_TRIANGLES, 0, 36);
		}
		glBindVertexArray(0);

		// === Dibujo del skybox ===
		glDepthFunc(GL_LEQUAL);
		skyboxShader.Use();
		view = glm::mat4(glm::mat3(camera.GetViewMatrix()));
		glUniformMatrix4fv(glGetUniformLocation(skyboxShader.Program, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(skyboxShader.Program, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

		glBindVertexArray(skyboxVAO);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
		glDrawArrays(GL_TRIANGLES, 0, 36);
		glBindVertexArray(0);
		glDepthFunc(GL_LESS);

		// Swap the screen buffers
		glfwSwapBuffers(window);
	}

	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
	glDeleteVertexArrays(1, &skyboxVAO);
	glDeleteBuffers(1, &skyboxVAO);
	delete g_SteveAnim;
	delete g_SteveAnimator;
	glfwTerminate();
	return 0;
}


void DoMovement()
{
	// Camera controls
	if (keys[GLFW_KEY_W] || keys[GLFW_KEY_UP]) camera.ProcessKeyboard(FORWARD, deltaTime);
	if (keys[GLFW_KEY_S] || keys[GLFW_KEY_DOWN]) camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (keys[GLFW_KEY_A] || keys[GLFW_KEY_LEFT]) camera.ProcessKeyboard(LEFT, deltaTime);
	if (keys[GLFW_KEY_D] || keys[GLFW_KEY_RIGHT]) camera.ProcessKeyboard(RIGHT, deltaTime);

}

// Is called whenever a key is pressed/released via GLFW
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	// Campana: C pausa o reanuda su bucle
	if (key == GLFW_KEY_C && action == GLFW_PRESS)
	{
		campanaPausada = !campanaPausada;
		printf("Campana: %s\n", campanaPausada ? "pausada" : "reanudada");
	}

	// Spotlights: R = roja, V = verde, B = azul
	if (key == GLFW_KEY_R && action == GLFW_PRESS) spotEncendida[0] = !spotEncendida[0];
	if (key == GLFW_KEY_V && action == GLFW_PRESS) spotEncendida[1] = !spotEncendida[1];
	if (key == GLFW_KEY_B && action == GLFW_PRESS) spotEncendida[2] = !spotEncendida[2];

	// Steve (solo referencia del profesor): se movio de B a N porque B es la luz azul
	if (key == GLFW_KEY_N && action == GLFW_PRESS)
	{
		stevePosZ = 0.0f;
		steveEstado = 0;
		steveDir = 1.0f;

		g_SteveAnimator->PlayAnimation(g_SteveAnim);
		playFBX = true;

		printf("Animaci�n de Steve iniciada\n");

	}
	if (key == GLFW_KEY_Z && action == GLFW_PRESS)
	{
		zorroCaminando = !zorroCaminando;
		printf("Zorro: %s\n", zorroCaminando ? "caminando" : "detenido");
	}
	if (key == GLFW_KEY_X && action == GLFW_PRESS && !zorroSaltando)
	{
		zorroSaltando = true;
		zorroSaltoT = 0.0f;
		printf("Zorro: salto\n");
	}
	if (key == GLFW_KEY_L && action == GLFW_PRESS)
	{
		luzNaranja = !luzNaranja;
		printf("Luz direccional: %s\n", luzNaranja ? "naranja" : "blanca");
	}
	if (GLFW_KEY_ESCAPE == key && GLFW_PRESS == action)
	{
		glfwSetWindowShouldClose(window, GL_TRUE);
	}
	if (key >= 0 && key < 1024)
	{
		if (action == GLFW_PRESS)
			keys[key] = true;
		else if (action == GLFW_RELEASE)
			keys[key] = false;
	}
}

// Function to handle keyframe animation logic
void AnimationKeys() {
	// === Modelos de la Revolucion: relojes de cada animacion ===
	tiempoTren += deltaTime;
	if (!campanaPausada)
		tiempoCampana += deltaTime;
	tiempoCanon += deltaTime;
	tiempoBandera += deltaTime;

	// === Zorro: caminata ===
	if (zorroCaminando)
	{
		float rotObjetivo = zorroDir > 0.0f ? 90.0f : 270.0f;
		if (zorroRot != rotObjetivo)
		{
			// Al llegar a un extremo del carril gira en su lugar antes de regresar
			float giro = zorroVelGiro * deltaTime;
			if (fabs(rotObjetivo - zorroRot) <= giro)
				zorroRot = rotObjetivo;
			else
				zorroRot += (rotObjetivo > zorroRot) ? giro : -giro;
		}
		else
		{
			zorroPosX += zorroDir * zorroVelocidad * deltaTime;
			if (zorroPosX >= zorroLimiteMaxX) { zorroPosX = zorroLimiteMaxX; zorroDir = -1.0f; }
			if (zorroPosX <= zorroLimiteMinX) { zorroPosX = zorroLimiteMinX; zorroDir = 1.0f; }
		}
		zorroCiclo += zorroFrecPaso * deltaTime;
	}
	// La amplitud del paso sube al caminar y baja al detenerse, asi las patas regresan suavemente
	float ampObjetivo = zorroCaminando ? 1.0f : 0.0f;
	zorroAmp += (ampObjetivo - zorroAmp) * glm::min(1.0f, 5.0f * deltaTime);

	// === Zorro: salto ===
	if (zorroSaltando)
	{
		zorroSaltoT += deltaTime / zorroDuracionSalto;
		if (zorroSaltoT >= 1.0f)
		{
			zorroSaltoT = 0.0f;
			zorroSaltando = false;
		}
	}
}

// Is called whenever the mouse moves
void MouseCallback(GLFWwindow* window, double xPos, double yPos)
{
	if (firstMouse)
	{
		lastX = xPos;
		lastY = yPos;
		firstMouse = false;
	}

	GLfloat xOffset = xPos - lastX;
	GLfloat yOffset = lastY - yPos;  // Reversed since y-coordinates go from bottom to left

	lastX = xPos;
	lastY = yPos;

	camera.ProcessMouseMovement(xOffset, yOffset);
}


void interpolation(void)
{

	KeyFrame[playIndex].incX = (KeyFrame[playIndex + 1].gatoPosX - KeyFrame[playIndex].gatoPosX) / i_max_steps;
	KeyFrame[playIndex].incY = (KeyFrame[playIndex + 1].gatoPosY - KeyFrame[playIndex].gatoPosY) / i_max_steps;
	KeyFrame[playIndex].incZ = (KeyFrame[playIndex + 1].gatoPosZ - KeyFrame[playIndex].gatoPosZ) / i_max_steps;
	KeyFrame[playIndex].headIncGato = (KeyFrame[playIndex + 1].headGato - KeyFrame[playIndex].headGato) / i_max_steps;
	KeyFrame[playIndex].FLegsIncGato = (KeyFrame[playIndex + 1].FLegsGato - KeyFrame[playIndex].FLegsGato) / i_max_steps;
	KeyFrame[playIndex].BLegsIncGato = (KeyFrame[playIndex + 1].BLegsGato - KeyFrame[playIndex].BLegsGato) / i_max_steps;
	KeyFrame[playIndex].tailIncGato = (KeyFrame[playIndex + 1].tailGato - KeyFrame[playIndex].tailGato) / i_max_steps;

	KeyFrame[playIndex].rotGatoInc = (KeyFrame[playIndex + 1].rotGato - KeyFrame[playIndex].rotGato) / i_max_steps;
	KeyFrame[playIndex].inclinacionIncGato = (KeyFrame[playIndex + 1].inclinacionGato - KeyFrame[playIndex].inclinacionGato) / i_max_steps;
}