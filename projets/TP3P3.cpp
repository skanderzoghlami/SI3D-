//! \file tuto_rayons.cpp

#include <vector>
#include <cfloat>
#include <chrono>
#include <random>

#include "vec.h"
#include "mat.h"
#include "color.h"
#include "image.h"
#include "image_io.h"
#include "image_hdr.h"
#include "orbiter.h"
#include "mesh.h"
#include "wavefront_fast.h"

// genere une direction sur l'hemisphere, 
// cf GI compendium, eq 34
Vector sample34(const float u1, const float u2)
{
    // coordonnees theta, phi
    float cos_theta = u1;
    float phi = float(2 * M_PI) * u2;

    // passage vers x, y, z
    float sin_theta = std::sqrt(1 - cos_theta * cos_theta);
    return Vector(std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta);
}

// evalue la densite de proba, la pdf de la direction, cf GI compendium, eq 34
float pdf34(const Vector& w)
{
    if (w.z < 0) return 0;
    return 1 / float(2 * M_PI);
}

// genere une direction sur l'hemisphere, 
// cf GI compendium, eq 35
Vector sample35(const float u1, const float u2)
{
    // coordonnees theta, phi
    float cos_theta = std::sqrt(u1);
    float phi = float(2 * M_PI) * u2;

    // passage vers x, y, z
    float sin_theta = std::sqrt(1 - cos_theta * cos_theta);
    return Vector(std::cos(phi) * sin_theta, std::sin(phi) * sin_theta, cos_theta);
}

// evalue la densite de proba, la pdf de la direction, cf GI compendium, eq 35
float pdf35(const Vector& w)
{
    if (w.z < 0) return 0;
    return w.z / float(M_PI);
}

struct World
{
    World(const Vector& _n) : n(_n)
    {
        float sign = std::copysign(1.0f, n.z);
        float a = -1.0f / (sign + n.z);
        float d = n.x * n.y * a;
        t = Vector(1.0f + sign * n.x * n.x * a, sign * d, -sign * n.x);
        b = Vector(d, sign + n.y * n.y * a, -n.y);
    }

    // transforme le vecteur du repere local vers le repere du monde
    Vector operator( ) (const Vector& local)  const { return local.x * t + local.y * b + local.z * n; }

    // transforme le vecteur du repere du monde vers le repere local
    Vector inverse(const Vector& global) const { return Vector(dot(global, t), dot(global, b), dot(global, n)); }

    Vector t;
    Vector b;
    Vector n;
};

struct Ray
{
    Point o;            // origine
    Vector d;           // direction
    float tmax;             // intervalle [0 tmax]

    Ray(const Point& _o, const Point& _e) : o(_o), d(Vector(_o, _e)) {}

    Ray(const Point& origine, const Vector& direction) : o(origine), d(direction), tmax(FLT_MAX) {}
};

struct Hit
{
    float t;            // p(t)= o + td, position du point d'intersection sur le rayon
    float u, v;         // p(u, v), position du point d'intersection sur le triangle
    int triangle_id;    // indice du triangle dans le mesh

    Hit() : t(FLT_MAX), u(), v(), triangle_id(-1) {}
    Hit(const float _t, const float _u, const float _v, const int _id) : t(_t), u(_u), v(_v), triangle_id(_id) {}
    operator bool() { return (triangle_id != -1); }
};

struct RayHit
{
    Point o;            // origine
    float t;            // p(t)= o + td, position du point d'intersection sur le rayon
    Vector d;           // direction
    int triangle_id;    // indice du triangle dans le mesh
    float u, v;
    int x, y;

    RayHit(const Point& _o, const Point& _e) : o(_o), t(1), d(Vector(_o, _e)), triangle_id(-1), u(), v(), x(), y() {}
    RayHit(const Point& _o, const Point& _e, const int _x, const int _y) : o(_o), t(1), d(Vector(_o, _e)), triangle_id(-1), u(), v(), x(_x), y(_y) {}
    operator bool() { return (triangle_id != -1); }
};


struct Triangle
{
    Point p;            // sommet a du triangle
    Vector e1, e2;      // aretes ab, ac du triangle
    int id;

    float aire;

    Triangle(const TriangleData& data, const int _id) : p(data.a), e1(Vector(data.a, data.b)), e2(Vector(data.a, data.c)), id(_id) {
        float ab = length(e1);
        float ac = length(e2);
        float bc = length((p + e2) - (p + e1));
        float p = (ab + ac + bc) / 2.f;
        aire = sqrt(p * (p - ab) * (p - bc) * (p - ac));
    }

    /* calcule l'intersection ray/triangle
        cf "fast, minimum storage ray-triangle intersection"

        renvoie faux s'il n'y a pas d'intersection valide (une intersection peut exister mais peut ne pas se trouver dans l'intervalle [0 tmax] du rayon.)
        renvoie vrai + les coordonnees barycentriques (u, v) du point d'intersection + sa position le long du rayon (t).
        convention barycentrique : p(u, v)= (1 - u - v) * a + u * b + v * c
    */
    Hit intersect(const Ray& ray, const float tmax) const
    {
        Vector pvec = cross(ray.d, e2);
        float det = dot(e1, pvec);

        float inv_det = 1 / det;
        Vector tvec(p, ray.o);

        float u = dot(tvec, pvec) * inv_det;
        if (u < 0 || u > 1) return Hit();

        Vector qvec = cross(tvec, e1);
        float v = dot(ray.d, qvec) * inv_det;
        if (v < 0 || u + v > 1) return Hit();

        float t = dot(e2, qvec) * inv_det;
        if (t > tmax || t < 0) return Hit();

        return Hit(t, u, v, id);           // p(u, v)= (1 - u - v) * a + u * b + v * c
    }

    //GI compemdium eq 18
    Point sample18(const float u1, const float u2) const
    {
        float r1 = std::sqrt(u1);
        float a = 1 - r1;
        float b = (1 - u2) * r1;
        float g = u2 * r1;
        return a * p + b * (p + e1) + g * (p + e2);
    }

    float pdf18(const Point& p) const
    {
        return 1.0 / aire;
    }

    void intersect(RayHit& ray) const
    {
        Vector pvec = cross(ray.d, e2);
        float det = dot(e1, pvec);

        float inv_det = 1 / det;
        Vector tvec(p, ray.o);

        float u = dot(tvec, pvec) * inv_det;
        if (u < 0 || u > 1) return;

        Vector qvec = cross(tvec, e1);
        float v = dot(ray.d, qvec) * inv_det;
        if (v < 0 || u + v > 1) return;

        float t = dot(e2, qvec) * inv_det;
        if (t < 0 || t > ray.t) return;

        // touche !!
        ray.t = t;
        ray.triangle_id = id;
        ray.u = u;
        ray.v = v;
    }
};

Vector normal(const Mesh& mesh, const Hit& hit)
{
    // recuperer le triangle complet dans le mesh
    const TriangleData& data = mesh.triangle(hit.triangle_id);
    // interpoler la normale avec les coordonn�es barycentriques du point d'intersection
    float w = 1 - hit.u - hit.v;
    Vector n = w * Vector(data.na) + hit.u * Vector(data.nb) + hit.v * Vector(data.nc);
    return normalize(n);
}

Color diffuse_color(const Mesh& mesh, const Hit& hit)
{
    const Material& material = mesh.triangle_material(hit.triangle_id);
    return material.diffuse;
}

struct Source
{
    Point s;
    Color emission;
    int triangle_id;
};



Color shade(const int N, std::uniform_real_distribution<float>& u01, std::default_random_engine& random, const Point o, const Vector n, const Mesh mesh,
    const std::vector<Triangle> triangles, const std::vector<Source> sources, const  std::vector<Color> diffuse, const Material mat) {
    Color finalColor = Color(0, 0, 0);
    for (unsigned int i = 0; i < sources.size(); i++) {
        for (int k = 0; k < N; k++) {
            float u1 = u01(random);
            float u2 = u01(random);
            // on génère un point random sur le triangle
            Point sourcePos = triangles[sources[i].triangle_id].sample18(u1, u2);
            Vector v = Vector(o, sourcePos);
            Ray r(o, normalize(v));
            //on vérifie qu'on voit bien la lumière depuis ce point
            Hit htemp;
            for (int j = 0; j < int(triangles.size()); j++)
            {
                if (Hit h = triangles[j].intersect(r, htemp.t)) {
                    htemp = h;
                }
            }
            if (htemp.triangle_id == sources[i].triangle_id) {
                //si c'est le cas on applique le calcul de l'éclairage direct
                Color fr = mat.diffuse / M_PI;
                Vector lightNormal = normal(mesh, htemp);
                finalColor = finalColor + sources[i].emission * fr
                    * ((std::fmax(0., dot(n, r.d)) * std::fmax(0., dot(lightNormal, -r.d))) / dot(v, v))
                    * triangles[htemp.triangle_id].aire;
            }
        }
    }
    return mat.emission + finalColor / N;
}

Color directe(const int N, std::uniform_real_distribution<float>& u01, std::default_random_engine& random, const Point o, const Vector n,
    const std::vector<Triangle> triangles) {
    World _w = World(n);
    float occultation = 0.;
    for (int i = 0; i < N; i++) {
        Ray r(o, _w(sample35(u01(random), u01(random))));
        Hit htemp;
        for (int j = 0; j < int(triangles.size()); j++)
        {
            if (Hit h = triangles[j].intersect(r, htemp.t)) {
                htemp = h;
            }
        }
        if (!htemp)
            occultation += 1.;
    }
    return Color(occultation / N);
}

int main(const int argc, const char** argv)
{
    const char* mesh_filename = "data/cornell.obj";
    if (argc > 1)
        mesh_filename = argv[1];

    const char* orbiter_filename = "data/cornell_orbiter.txt";;
    if (argc > 2)
        orbiter_filename = argv[2];

    Orbiter camera;
    if (camera.read_orbiter(orbiter_filename) < 0)
        return 1;

    Mesh mesh = read_mesh_fast(mesh_filename);


    // recupere les triangles
    std::vector<Triangle> triangles;
    {
        int n = mesh.triangle_count();
        for (int i = 0; i < n; i++)
            triangles.emplace_back(mesh.triangle(i), i);
    }

    // recupere les materiaux diffus
    std::vector<Color> diffuse;

    // recupere les sources
    std::vector<Source> sources;
    {
        int n = mesh.triangle_count();
        for (int i = 0; i < n; i++)
        {
            const Material& material = mesh.triangle_material(i);
            diffuse.push_back(material.diffuse);
            if (material.emission.r + material.emission.g + material.emission.b > 0)
            {
                // utiliser le centre du triangle comme source de lumi�re
                const TriangleData& data = mesh.triangle(i);
                Point p = (Point(data.a) + Point(data.b) + Point(data.c)) / 3;

                sources.push_back({ p, material.emission, i });
            }
        }

        printf("%d sources\n", int(sources.size()));
        assert(sources.size() > 0);
    }


    Image image(1024, 768);

    // recupere les transformations
    camera.projection(image.width(), image.height(), 45);
    Transform model = Identity();
    Transform view = camera.view();
    Transform projection = camera.projection();
    Transform viewport = camera.viewport();
    Transform inv = Inverse(viewport * projection * view * model);

    auto startA = std::chrono::high_resolution_clock::now();

    srand(time(0));
    std::random_device seed;
    std::default_random_engine random(seed());
    std::uniform_real_distribution<float> u01(0.f, 1.f);

#pragma omp parallel for schedule(dynamic, 1)
    for (int y = 0; y < image.height(); y++)
        for (int x = 0; x < image.width(); x++)
        {
            // generer le rayon
            Point origine = inv(Point(x + .5f, y + .5f, 0));
            Point extremite = inv(Point(x + .5f, y + .5f, 1));
            Ray ray(origine, extremite);

            // rays.emplace_back(origine, extremite, x, y);

            Hit hit;
            for (int i = 0; i < int(triangles.size()); i++)
            {
                if (Hit h = triangles[i].intersect(ray, hit.t))
                    // ne conserve que l'intersection la plus proche de l'origine du rayon
                    hit = h;
            }

            if (hit)
            {
                Vector n = normal(mesh, hit);
                const Material& mat = mesh.triangle_material(hit.triangle_id);
                Point p = triangles[hit.triangle_id].p * (1 - hit.u - hit.v) +
                    (triangles[hit.triangle_id].p + triangles[hit.triangle_id].e1) * hit.u +
                    (triangles[hit.triangle_id].p + triangles[hit.triangle_id].e2) * hit.v;
                Point o = p + 0.001 * n;

                // Vrai Sources de lumiére
                image(x, y) = shade(64, u01, random, o, n, mesh, triangles, sources, diffuse, mat);


                // Application directe de l'eq de rendu
                //image(x, y) = directe(64, u01, random, o, n, triangles);
            }
        }
    auto stopA = std::chrono::high_resolution_clock::now();
    int cpu = std::chrono::duration_cast<std::chrono::milliseconds>(stopA - startA).count();
    printf("%dms\n", cpu);

    write_image(image, "render.png");
    write_image_hdr(image, "shadow.hdr");
    return 0;
}

