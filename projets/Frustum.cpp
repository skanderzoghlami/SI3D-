// Frustum Culling


#include "wavefront.h"
#include "texture.h"

#include "orbiter.h"
#include "draw.h"
#include "app_camera.h"        // classe Application a deriver
#include "program.h"
#include "texture.h"
#include "uniforms.h"
#include <texture.h>

GLuint program;


// Structure of the bbox , we need only the center
struct BoundingBox {
    float width;
    float height;
    float depth;
    float centerX;
    float centerY;
    float centerZ;
};
// pmin, pmax => Bbox
BoundingBox CalculateBoundingBox(Point pmin, Point pmax) {
    BoundingBox bbox;

    bbox.width = pmax.x - pmin.x;
    bbox.height = pmax.y - pmin.y;
    bbox.depth = pmax.z - pmin.z;

    bbox.centerX = (pmax.x + pmin.x) / 2;
    bbox.centerY = (pmax.y + pmin.y) / 2;
    bbox.centerZ = (pmax.z + pmin.z) / 2;

    return bbox;
}

struct Bbox_Mesh {
    BoundingBox bbox;
    Mesh mesh;
    std::vector<TriangleGroup> tgs;
    std::vector<unsigned int> triangle_properties;
};

// Function to subdivide a bounding box into multiple bboxes
std::vector<BoundingBox> SubdivideBoundingBox(const BoundingBox& bbox, int divisionsX, int divisionsY, int divisionsZ) {
    std::vector<BoundingBox> subBoundingBoxes;

    // Calculate the dimensions of each sub-bounding box
    float subWidth = bbox.width / divisionsX;
    float subHeight = bbox.height / divisionsY;
    float subDepth = bbox.depth / divisionsZ;

    // Calculate the positions of each sub-bounding box
    for (int i = 0; i < divisionsX; ++i) {
        for (int j = 0; j < divisionsY; ++j) {
            for (int k = 0; k < divisionsZ; ++k) {
                BoundingBox subBox;
                subBox.width = subWidth;
                subBox.height = subHeight;
                subBox.depth = subDepth;
                subBox.centerX = bbox.centerX - bbox.width / 2 + (i + 0.5) * subWidth;
                subBox.centerY = bbox.centerY - bbox.height / 2 + (j + 0.5) * subHeight;
                subBox.centerZ = bbox.centerZ - bbox.depth / 2 + (k + 0.5) * subDepth;

                subBoundingBoxes.push_back(subBox);
            }
        }
    }
    return subBoundingBoxes;
}

Point CalculateTriangleCenter(const Point& vertex1, const Point& vertex2, const Point& vertex3) {
    // Determine the minimum and maximum extents of the triangle
    float min_x = std::min({ vertex1.x, vertex2.x, vertex3.x });
    float min_y = std::min({ vertex1.y, vertex2.y, vertex3.y });
    float min_z = std::min({ vertex1.z, vertex2.z, vertex3.z });
    float max_x = std::max({ vertex1.x, vertex2.x, vertex3.x });
    float max_y = std::max({ vertex1.y, vertex2.y, vertex3.y });
    float max_z = std::max({ vertex1.z, vertex2.z, vertex3.z });

    // Calculate the dimensions of the bounding box
    float width = max_x - min_x;
    float height = max_y - min_y;
    float depth = max_z - min_z;

    // Calculate the center of the bounding box
    float centerX = min_x + width / 2;
    float centerY = min_y + height / 2;
    float centerZ = min_z + depth / 2;

    Point center;
    center.x = centerX;
    center.y = centerY;
    center.z = centerZ;

    return center;
}

// Function to check if a point is inside a bounding box
bool IsPointInsideBoundingBox(const Point& point, const BoundingBox& bbox) {
    return (point.x >= bbox.centerX - bbox.width / 2 && point.x <= bbox.centerX + bbox.width / 2 &&
        point.y >= bbox.centerY - bbox.height / 2 && point.y <= bbox.centerY + bbox.height / 2 &&
        point.z >= bbox.centerZ - bbox.depth / 2 && point.z <= bbox.centerZ + bbox.depth / 2);
}



bool isPointInsideFrostum(const Point& point, Transform& mvp) {
    Point clip_space_point = mvp(point);
    // Check if the point is in the canonical view volume
    if (clip_space_point.x >= -1.0 && clip_space_point.x <= 1.0 &&
        clip_space_point.y >= -1.0 && clip_space_point.y <= 1.0 &&
        clip_space_point.z >= -1.0 && clip_space_point.z <= 1.0) {
        return true;
    }
    else {
        // The point is outside the camera frustum.
        return false;
    }
}








class TP : public AppCamera
{
public:
    // constructeur : donner les dimensions de l'image, et eventuellement la version d'openGL.
    TP() : AppCamera(1024, 640) {}

    // creation des objets de l'application
    int init()
    {
        //m_world = read_mesh("bistro-small/export.obj");
         m_world = read_mesh("data/bistro/exterior.obj");

        Point pmin, pmax;
        m_world.bounds(pmin, pmax);
        m_program = read_program("shaders/realscene.glsl");
        // regler la camera pour observer l'objet
        camera().lookat(pmin, pmax);
        if (m_world.materials().count() == 0)
            // pas de matieres, pas d'affichage
            return -1;
        if (m_world.has_texcoord() == false)
            // pas de texcoords, pas d'affichage
            return -1;
        Materials& materials = m_world.materials();
        if (materials.filename_count() == 0)
            // pas de textures, pas d'affichage
            return -1;
        //// charge les textures referencees par les matieres
        m_textures.resize(materials.filename_count(), 0);
        {
            for (unsigned i = 0; i < m_textures.size(); i++)
            {
                m_textures[i] = read_texture(0, materials.filename(i));

                // repetition des textures si les texccord sont > 1, au lieu de clamp...
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                // meilleure qualite de filtrage...
                // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 8);
            }

        }
        //// charge aussi une texture neutre pour les matieres sans texture...
        m_white_texture = read_texture(0, "data/grid.png");  // !! utiliser une vraie image blanche...


        BoundingBox bb = CalculateBoundingBox(pmin, pmax);
        std::vector<BoundingBox> subBbs = SubdivideBoundingBox(bb, 2, 2, 2);

        // Not Yet Tested

        for (int i = 0; i < subBbs.size(); i++) {
            Mesh mesh(GL_TRIANGLES);
            Bbox_Mesh new_bbm;
            new_bbm.mesh = mesh;
            new_bbm.mesh.m_triangle_materials = m_world.m_triangle_materials;
            new_bbm.bbox = subBbs[i];
            new_bbm.mesh.materials(m_world.materials());
            bbms.push_back(new_bbm);
        }



        for (int j = 0; j < m_world.triangle_count(); ++j) {
            TriangleData triangle = m_world.triangle(j);
            Point centerTriangle = CalculateTriangleCenter(triangle.a, triangle.b, triangle.c);
            int i = 0, v = false;
            while (i < subBbs.size() && !v) {
                if (IsPointInsideBoundingBox(centerTriangle, subBbs[i])) {
                    unsigned int a = bbms[i].mesh.vertex(vec3(triangle.a.x, triangle.a.y, triangle.a.z));
                    bbms[i].mesh.texcoord(triangle.ta).normal((triangle.na));
                    unsigned int b = bbms[i].mesh.vertex(vec3(triangle.b.x, triangle.b.y, triangle.b.z));
                    bbms[i].mesh.texcoord(triangle.tb).normal((triangle.nb));
                    unsigned int c = bbms[i].mesh.vertex(vec3(triangle.c.x, triangle.c.y, triangle.c.z));
                    bbms[i].mesh.texcoord(triangle.tc).normal((triangle.nc));
                    bbms[i].mesh.triangle(a, b, c);
                    bbms[i].triangle_properties.push_back(m_world.m_triangle_materials[j]);
                    v = true;
                }
                i++;
            }
        }
        for (int i = 0; i < bbms.size(); ++i) {
            if (bbms[i].triangle_properties.size())
                bbms[i].tgs = bbms[i].mesh.groups(bbms[i].triangle_properties);
        }



        glClearColor(0.2f, 0.2f, 0.2f, 1.f);        // couleur par defaut de la fenetre
        glClearDepth(1.f);                          // profondeur par defaut
        glDepthFunc(GL_LESS);                       // ztest, conserver l'intersection la plus proche de la camera
        glEnable(GL_DEPTH_TEST);                    // activer le ztest
        //glEnable(GL_CULL_FACE);

        return 0;
    }

    // destruction des objets de l'application
    int quit()
    {
        m_world.release();
        release_program(m_program);

        return 0;   // pas d'erreur
    }

    // dessiner une nouvelle image
    int render()
    {
        if (key_state('s'))
        {
            clear_key_state('s');
            static int id = 1;
            screenshot("camera", id++);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(m_program);

        Transform model = RotationX(0);
        Transform view = m_camera.view();
        Transform projection = m_camera.projection(window_width(), window_height(), 45);

        //// . composer les transformations : model, view et projection
        Transform mvp = projection * view * model;
        Transform mv = projection * view;

        program_uniform(m_program, "mvpMatrix", mvp);

        //// afficher chaque groupe
        for (unsigned i = 0; i < bbms.size(); i++) {
            for (unsigned j = 0; j < bbms[i].tgs.size(); j++)
            {
                if (isPointInsideFrostum(Point(bbms[i].bbox.centerX, bbms[i].bbox.centerY, bbms[i].bbox.centerZ), mvp)) {
                    const Materials& materials = bbms[i].mesh.materials();
                    const Material& material = materials(bbms[i].tgs[j].index);
                    // printf("%d index corresponding to group  %d of bbm %d .\n", bbms[i].tgs[j].index , j,i);
                     //   couleur de la matiere du groupe de triangle
                     //program_uniform(m_program, "material_color", material.diffuse);
                     //   texture definie par la matiere, ou texture neutre sinon...
                    if (material.diffuse_texture != -1)
                    {
                        program_use_texture(m_program, "material_texture", 0, m_textures[material.diffuse_texture]);


                    }
                    else {
                        program_use_texture(m_program, "material_texture", 0, m_white_texture);
                    }

                    // 1 draw par groupe de triangles...
                    bbms[i].mesh.draw(bbms[i].tgs[j].first, bbms[i].tgs[j].n, m_program, /* use position */ true,
                        /* use texcoord */ true, /* use normal */ true, /* use color */ false, /* use material index*/ false);
        

                }
            }
        }
        //draw(m_world, /* model */ Identity(), camera());

        return 1;
    }

protected:
    Mesh m_world;
    std::vector<GLuint> m_textures;
    GLuint m_white_texture;
    std::vector<TriangleGroup> m_groups;
    GLuint m_program;
    std::vector<Bbox_Mesh> bbms;
};


int main(int argc, char** argv)
{
    TP tp;
    tp.run();
    return 0;
}

