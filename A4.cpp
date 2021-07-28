// Termm--Fall 2020

#include <glm/ext.hpp>

#include "A4.hpp"
#include "GeometryNode.hpp"
#include "cs488-framework/MathUtils.hpp"

// #define FAST_MODE

using namespace glm;
using namespace std;

float AIR_REFRACTIVE_IDX = 1.0f;
float GLASS_REFRACTIVE_IDX = 1.4f;
float WATER_REFRACTIVE_IDX = 1.3f;

int duration = 15;
int fps = 24;
int sample = 3;
int state = 0;
int maxHitTimes = 3;

float d[] = {1.0f, 2.0f, 5.0f, 5.0f};
float lastTime = 0;
vec3 redDir = vec3();
vec3 blueDir = vec3();
vec3 glassDir = normalize(vec3(-1, 0, 1.1));

bool hit1 = false;
bool hit2 = false;
float velocityRatio1 = 0;
float velocityRatio2 = 0;
float initialVelocity = 700.0f;

static vec3 backgroundColorRB = vec3(0.45f, 0.28f, 0.60f);
static vec3 backgroundColorLU = vec3(0.09f, 0.42f, 0.75f);
static vec3 backgroundStar = vec3(1.00f, 1.00f, 0.75f);

vec3 getTexture(Image *texture, float u, float v) {
	float w = texture->width();
	float h = texture->height();
	float di = (w - 1) * u;
	float dj = (h - 1) * v;
	float i = int(di);
	float j = int(dj);
	float up = di - i;
	float vp = dj - j;
	vec3 C00 = texture->getColor(i, j);
	vec3 C01 = texture->getColor(i, j + 1);
	vec3 C10 = texture->getColor(i + 1, j);
	vec3 C11 = texture->getColor(i + 1, j + 1);
	return C00 * (1 - up) * (1 - vp) + C01 * (1 - up) * vp + C10 * (1 - vp) * up + C11 * up * vp;
}

vec3 getBackgroundColor(vec3 bgrDir, Image *backgroundImage) {
	// return vec3();
	float pitch = atan(bgrDir.z / bgrDir.x);
	if (bgrDir.x < 0) {
		pitch += PI;

	} else if (bgrDir.z < 0) pitch += 2 * PI;
	float u = (pitch ) / (PI  * 2 );
	float xz = sqrt(pow(bgrDir.x, 2) + pow(bgrDir.z, 2));
	float v = 1.0f - (atan(bgrDir.y / xz) + PI / 2) / PI;
	// if (y == ny / 2 && x == nx / 2) cout << "u " << u << " v " << v << " dir " << radiansToDegrees(pitch) << " x " << bgrDir.x << " z " << bgrDir.z << endl;
	// cout << "u " << u << " v " << v << endl;
	return  getTexture(backgroundImage, u, v);
}

vec3 rayTrace(Ray &ray, SceneNode *root, Image *backgroundImage, vec3 ambient, const list<Light *> lights, int maxHit, bool refracted = false) {
	Intersection inter = root->intersect(ray);
	vec3 backgroundColor = getBackgroundColor(ray.direction, backgroundImage);
	vec3 color = backgroundColor;

	if (inter.hit) {
		vec3 matColor = inter.mat->m_kd;

		if (inter.mat->m_texture != nullptr) {
			matColor = getTexture(inter.mat->m_texture, inter.u, inter.v);
			// cout << "i: " << i << " j: " << j << endl;
			//
			// cout << "w: " << inter.mat->m_texture->width() << " h: " << inter.mat->m_texture->height() << endl;
			// cout << to_string(C00) << endl;
		}
		color = matColor * ambient;
		// if (sqrt(dot(matColor, matColor)) < 0.01f) cout << "black dot: pos " << to_string(inter.position) << " maxhit " << maxHit << endl;

		for(const Light * light : lights) {
			vec3 lightColor = vec3(0);
			// shadow ray
			vec3 lightDir = normalize(light->position - inter.position);

		  Ray shadowRay = Ray(inter.position, lightDir);
			Intersection shadowInter = root->intersect(shadowRay);
		//	std::cout << "shadowInter.hit" << shadowInter.hit << '\n';
			if (!shadowInter.hit) {
				// light contribute to colour
				// diffuse

				float n_dot_l = dot(inter.normal, lightDir);
				if (n_dot_l < 0) n_dot_l = 0;

				vec3 diffuse = matColor * n_dot_l;
				lightColor += light->colour * diffuse;

				// Specular
				float n_dot_h = dot(inter.normal * normalize(lightDir - ray.direction), -ray.direction);
				if (n_dot_h == 0) cout << "pos: " << to_string(inter.position) << " normal: " << to_string(inter.normal) << endl;
				if (n_dot_h < 0) n_dot_h = 0;
				float pf = pow(n_dot_h, inter.mat->m_shininess);
				vec3 specular = inter.mat->m_ks * pf;
				lightColor += light->colour * specular;
			}
			float c1 = 1000;
			float c2 = 100;
			float c3 = 1;
			float lightIntensity = 300000;
			float r = distance(light->position, inter.position);
			color += lightColor * std::min(lightIntensity / (c1 + c2 * r + c3 * pow(r, 2)), 1.0d);
		}
		// Reflect
		#ifndef FAST_MODE

		if (maxHit > 0) {
			float FR = 1.0f;
			vec3 reflection = vec3(0);
			vec3 refraction = vec3(0);
			vec3 offsetPos = inter.position;
			if (inter.mat->m_reflectivity != 0 ) {
				// Reflection
				vec3 reflectDir = normalize(ray.direction - 2 * inter.normal * dot(ray.direction, inter.normal));
				// float fudge = 0.00f;
				Ray reflectRay = Ray(offsetPos, reflectDir);
				reflection = inter.mat->m_reflectivity * rayTrace(reflectRay, root, backgroundImage, ambient, lights, maxHit - 1);
				// FR = ;
			}
			if (inter.mat->m_refraction != 0) {
				// Refraction
				vec3 i = ray.direction;
				vec3 n = inter.normal;
				float etai = AIR_REFRACTIVE_IDX;
				float etat = inter.mat->m_refraction;
				if (refracted) std::swap(etai, etat);
				float eta = etai / etat;
				float internalRefraction = 1 - pow(eta, 2) * (1 - pow(dot(i, n), 2));

				if (internalRefraction >= 0) {
					vec3 t = normalize((-eta * dot(i, n) - sqrt(internalRefraction)) * n + eta * i);
					offsetPos = inter.position;
					Ray refractRay = Ray(offsetPos, t);
					int nextMaxHit = maxHit;
					if (refracted) nextMaxHit -= 1;
					refraction = rayTrace(refractRay, root, backgroundImage, ambient, lights, nextMaxHit, !refracted);

					float thetai = dot(i, n);
					float thetat = asin(sin(thetai) * eta);
					float etai2 = pow(etai, 2);
					float etat2 = pow(etat, 2);
					float cit = cos(thetai) * cos(thetat);
					float nit = etai * etat;
					FR = pow((etai2 - etat2), 2) * pow(cit, 2) + pow(cos(pow(thetai, 2)) - cos(pow(thetat, 2)), 2) * pow(nit, 2) /
										pow(cit * (etai2 + etat2) + nit * (cos(pow(thetai, 2)) + cos(pow(thetat, 2))), 2);
				}
			}

			float FT = 1 - FR;
			if (inter.mat->m_reflectivity == 0) FT = 1.0;
			color += inter.mat->m_ks * (reflection * abs(FR) + refraction * abs(FT));
			// if (sqrt(dot(color, color)) < 0.05f) cout << "black dot: FT " << FT << " FR " << FR << " refle " << to_string(reflection) << " refra " << to_string(refraction) <<endl;
		}
		#endif
	}

	return color;
}

SceneNode* getNode(SceneNode& root, string name){
	if(root.m_name == name){
		return &root;
	}
	for(SceneNode * child : root.children){
		SceneNode * res = getNode(*child, name);
		if(res!= NULL){
			return res;
		}
	}
	return NULL;
}

vec3 transformVec(mat4 trans, vec3 vector) {
	return vec3(trans * vec4(vector, 1.0f));
}

void updateMovement(SceneNode * root, int frame, glm::vec3 & eye, glm::vec3 & view, glm::vec3 & up) {
	float frameFactor = fps * d[state];
  // 	cout << "frame: " << frame << " lastTime: " << lastTime << " state: " << state << " hit " << hit1 << " hit2 " << hit2 << endl;
	if (state == 0) {
		view = transformVec(translate(vec3(0, -1, 0) * 600.0f / frameFactor), view);

	} else if (state == 1) {
		eye = transformVec(translate(vec3(-3, -2.5, 0) * 300.0f / frameFactor), eye);
		view = transformVec(translate(vec3(0, 1, 0) * 200.0f / frameFactor), view);
	} else if (state == 2) {
		eye = transformVec(translate(vec3(-1, 0, 0) * 350.0f / frameFactor), eye);
		view = transformVec(translate(vec3(-1, 0, 0) * 350.0f / frameFactor), view);
		GeometryNode *glassballNode = static_cast<GeometryNode*>(getNode(*root, "glassball"));
		GeometryNode *redballNode = static_cast<GeometryNode*>(getNode(*root, "redball"));
		NonhierSphere *glassball = static_cast<NonhierSphere*>(glassballNode->m_primitive);
		NonhierSphere *redball = static_cast<NonhierSphere*>(redballNode->m_primitive);
		if (!hit1) {
			if (glassballNode != nullptr && redballNode != nullptr) {
				glassballNode->translate(glassDir * initialVelocity / frameFactor);

				vec3 redPos = vec3(redballNode->trans * vec4(0.0f, 0.0f, 0.0f, 1.0f));
				vec3 glassPos = vec3(glassballNode->trans * vec4(0.0f, 0.0f, 0.0f, 1.0f));
				float radiusSum = redball->m_radius + glassball->m_radius;
				// cout << "dist " << distance(redPos, glassPos) << " radiusSum " << radiusSum << endl;
				if (distance(redPos, glassPos) <= radiusSum) {
					hit1 = true;
					vec3 n = normalize(glassPos - redPos);
					glassDir = normalize(glassDir - 2 * n * dot(glassDir, n));
					redDir = -n;
					velocityRatio1 = 1 - abs(sin(dot(glassDir, n)));
					cout << "vr1 " << velocityRatio1 << endl;
					cout << "glassDir " << to_string(glassDir) << endl;
					lastTime = 1000;
				}
			}
		} else {
				glassballNode->translate(glassDir * initialVelocity * velocityRatio1 / frameFactor);
				vec3 shift = redDir * initialVelocity * (1 - velocityRatio1) / frameFactor;
				redballNode->translate(shift);
				// float rotateSpd = sqrt(dot(shift, shift)) / redball->m_radius;
				redballNode->rotate('z', (float) radiansToDegrees(-shift.x / redball->m_radius));
				redballNode->rotate('x', (float) radiansToDegrees(shift.z / redball->m_radius));
				cout << "glass translate " << to_string(glassDir * initialVelocity * velocityRatio1 / frameFactor) << endl;
				cout << " shiftx: " << shift.x << " shiftz: " << shift.z << endl;
				GeometryNode *blueballNode = static_cast<GeometryNode*>(getNode(*root, "blueball"));
				NonhierSphere *blueball = static_cast<NonhierSphere*>(blueballNode->m_primitive);
				vec3 bluePos = vec3(blueballNode->trans * vec4(0.0f, 0.0f, 0.0f, 1.0f));
				vec3 redPos = vec3(redballNode->trans * vec4(0.0f, 0.0f, 0.0f, 1.0f));
				float radiusSum = blueball->m_radius + glassball->m_radius;
				if (distance(bluePos, redPos) <= radiusSum) {
					hit2 = true;
					vec3 n = normalize(redPos - bluePos);
					redDir = normalize(redDir - 2 * n * dot(redDir, n));
					blueDir = -n;
					velocityRatio2 = 1 - abs(sin(dot(redDir, n)));
					cout << "redDir " << to_string(redDir) << endl;
					cout << "blueDir " << to_string(blueDir) << endl;
					cout << "vr2 " << velocityRatio2 << endl;
					state++;

				}
		}
	} else if (state == 3) {
		eye = transformVec(translate(vec3(-1, 0, 0) * 350.0f / frameFactor), eye);
		view = transformVec(translate(vec3(-1, 0, 0) * 350.0f / frameFactor), view);
		GeometryNode *redballNode = static_cast<GeometryNode*>(getNode(*root, "redball"));
		NonhierSphere *redball = static_cast<NonhierSphere*>(redballNode->m_primitive);
		GeometryNode *blueballNode = static_cast<GeometryNode*>(getNode(*root, "blueball"));
		NonhierSphere *blueball = static_cast<NonhierSphere*>(blueballNode->m_primitive);
		redballNode->translate(redDir * initialVelocity * velocityRatio1 * velocityRatio2 / frameFactor);
		vec3 shift = blueDir * initialVelocity * velocityRatio1 * (1 - velocityRatio2) / frameFactor;
		blueballNode->translate(shift);
		cout << " shiftx: " << shift.x << " shiftz: " << shift.z << endl;
		blueballNode->rotate('z', (float) radiansToDegrees(-shift.x / blueball->m_radius));
		blueballNode->rotate('x', (float) radiansToDegrees(shift.z / blueball->m_radius));
	}

	if (frame / fps - lastTime >= d[state]) {

		lastTime += d[state];
		state++;

	}
	// SceneNode *redball = getNode(*root, "redball");
	// SceneNode *blueball = getNode(*root, "blueball");
	// SceneNode *lens = getNode(*root, "lens");
	//
	// if (redball != nullptr && blueball != nullptr) {
	// 	redball->rotate('y', -3.0f / fps);
	// 	redball->rotate('x', -3.0f / fps);
	// 	// redball->translate(vec3(-10, 0, -10) / fps);
	// 	// redball->translate(vec3(0, 20, 0));
	// 	// blueball->translate(vec3(0, 20, 0));
	//
	// 	// blueball->rotate('y', 3.0f);
	// 	// blueball->rotate('x', 3.0f);
	// }
	// if (lens != nullptr) {
	// 	lens->translate(vec3(-3, 0, 0) / fps);
	// }

	// cout << to_string(view) << endl;
}

void transTree(SceneNode *root) {
	for(SceneNode *child: root->children) {
		child->trans = root->trans * child->trans;
		child->invtrans = inverse(child->trans);
		transTree(child);
	}
}

void A4_Render(
		// What to render
		SceneNode * root,

		// Image to write to, set to a given width and height
		Image & image,

		// Viewing parameters
		glm::vec3 & eye,
		glm::vec3 & view,
		glm::vec3 & up,
		double fovy,

		// Lighting parameters
		const glm::vec3 & ambient,
		const std::list<Light *> & lights
) {

  // Fill in raytracing code here...

  std::cout << "F20: Calling A4_Render(\n" <<
		  "\t" << *root <<
          "\t" << "Image(width:" << image.width() << ", height:" << image.height() << ")\n"
          "\t" << "eye:  " << glm::to_string(eye) << std::endl <<
		  "\t" << "view: " << glm::to_string(view) << std::endl <<
		  "\t" << "up:   " << glm::to_string(up) << std::endl <<
		  "\t" << "fovy: " << fovy << std::endl <<
          "\t" << "ambient: " << glm::to_string(ambient) << std::endl <<
		  "\t" << "lights{" << std::endl;

	for(const Light * light : lights) {
		std::cout << "\t\t" <<  *light << std::endl;
	}
	std::cout << "\t}" << std::endl;
	std:: cout <<")" << std::endl;

	transTree(root);
	//root->applyTrans(mat4(1));
	// transform to world
	int ny = image.height();
	int nx = image.width();
	#ifdef FAST_MODE
	ny /= 5;
	nx /= 5;
	#endif



	std::cout << "Rendering: " << endl;
	int loading = 0;

	Image *backgroundImage = new Image();
	backgroundImage->loadPng("Assets/textures/castle2k.png");

	// render frames


	Image im(nx, ny);
	int nofFrames = fps * duration;
	string path =  "render/frame";
	string suffix =  ".png";
	for (uint frame = 0; frame < nofFrames; frame++) {
		float d = distance(eye, view);
		mat4 T1 = translate(mat4(1), vec3((float)-nx/2, (float)-ny/2, d));
		float height = 2 * d * tan(radians(fovy/2));
		// cout << "height: " << height << endl;
		float width = nx/ny * height;

		// cout << "ny: " << ny << endl;
		// cout << "d: " << d << endl;
		mat4 S2 = scale(mat4(1), vec3(-height/ny, -width/nx, 1.0f));
		vec3 w = normalize(view - eye);
		vec3 u = normalize(cross(up, w));
		vec3 v = cross(w, u);

		mat4 R3 = mat4{
			vec4(u, 0.0f),
			vec4(v, 0.0f),
			vec4(w, 0.0f),
			vec4(0.0f, 0.0f, 0.0f, 1.0f)
		};

		mat4 T4 = mat4(1);
		T4 = mat4{
			vec4(1.0f, 0.0f, 0.0f, 0.0f),
			vec4(0.0f, 1.0f, 0.0f, 0.0f),
			vec4(0.0f, 0.0f, 1.0f, 0.0f),
			vec4(eye, 1.0f)
		};


		mat4 pworld = T4 * R3 * S2 * T1;

		vec4 lookFrom = vec4(eye, 1.0f);
		if (frame - loading > nofFrames / 10) {
			std::cout << "█ " << (double) loading / nofFrames * 100 << "%" << endl;
			loading = frame;
		}
		for (uint y = 0; y < ny; ++y) {
			if (nofFrames == 1 && frame - loading > ny / 10) {
				std::cout << "█ " << (double) loading / ny * 100 << "%" << endl;
				loading = y;
			}
			for (uint x = 0; x < nx; ++x) {
				Ray ray = Ray();
				vec3 color = vec3(0);
				 // cout << "calculation" << endl;
				 ray.source = eye;
				 for(float i = 0; i < sample; i++){
	 				for(float j = 0; j < sample; j++){
						ray.direction = vec3(normalize(pworld * vec4((float)x + rand() % 100 / 50 - 1.2f, (float)y + rand() % 100 / 50 - 1.2f, 0.0f, 1.0f) - lookFrom));
	 					color += rayTrace(ray, root, backgroundImage, ambient, lights, maxHitTimes);
	 				}
	 			}
				color /= (float) pow(sample, 2);


				// cout << "screen " << to_string(pworld * vec4((float)x, (float)y, 0.0f, 1.0f)) << endl;
				// cout << "lookfrom " << to_string(lookFrom) << endl;
				// cout << lookFrom.x <<lookFrom.y << lookFrom.z<< endl;
				// cout << ray.direction.x <<ray.direction.y << ray.direction.z<< endl;
				//  cout << "1" << endl;
				// vec3 backgroundColor = ((float)x / nx + y / ny) / 2 * backgroundColorRB + ((float)(nx - x - 1) / nx + (ny - y - 1) / ny) / 2 * backgroundColorLU;
				// int distX = std::min(x, nx - x);
				// int distY = std::min(y, ny - y);
				// //  cout << "2" << endl;
				// float ratio = (float) ((distX * 2 * distY * 2) + (nx * ny)) / (2 * nx * ny);
				// //  cout << x << " " << y << " " << ratio << endl;
				//  // cout << "ratio" << ratio << endl;
				// if ((int)(500.0f * (1.0f - ratio)) != 0 && rand() % (int) (500 * (1 - ratio)) == 1) backgroundColor = ratio * backgroundStar;
				// cout << "raytracing" << endl;


				// Red:
				im(x, y, 0) = color.x;
				// Green:
				im(x, y, 1) = color.y;
				// Blue:
				im(x, y, 2) = color.z;
			}
		}
		string index = "";
		int digits = frame == 0? 0 : log10(frame);
		for (int i = 0; i < 3 - digits; i++) {
			index += "0";
		}
		cout << path + index + to_string(frame) << endl;
		im.savePng(path + index + to_string(frame) + suffix);
		updateMovement(root, frame, eye, view, up);
		// cout << to_string(view) << endl;
	}

	std::cout << "█ 100.0% DONE" << std::endl;
}
