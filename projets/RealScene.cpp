
//! Real Scene


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












class TP : public AppCamera
{
public:
    // constructeur : donner les dimensions de l'image, et eventuellement la version d'openGL.
    TP() : AppCamera(1024, 640) {}

    // creation des objets de l'application
    int init()
    {

        m_world = read_mesh("bistro-small/export.obj");
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

            printf("%d textures.\n", materials.filename_count());
        }
        //// charge aussi une texture neutre pour les matieres sans texture...
        m_white_texture = read_texture(0, "data/grid.png");  // !! utiliser une vraie image blanche...


        BoundingBox bb = CalculateBoundingBox(pmin, pmax);
        std::vector<BoundingBox> subBbs = SubdivideBoundingBox(bb, 2, 2, 2);

        std::vector<unsigned int> bboxCoordinates;

        for (int i = 0; i < m_world.triangle_count(); ++i) {
            TriangleData triangle = m_world.triangle(i);
            Point centerTriangle = CalculateTriangleCenter(triangle.a, triangle.b, triangle.c);
            int j = 0;
            bool v = false;
            while (j < subBbs.size() && v == false) {
                if (IsPointInsideBoundingBox(centerTriangle, subBbs[j])) {
                    v = true;
                    bboxCoordinates.push_back(j);
                }
                j++;
            }
            if (v == false) {
                bboxCoordinates.push_back(0);
            }
        }

        m_groups = m_world.groups();


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
        return 0;   // pas d'erreur
    }

    // dessiner une nouvelle image
    int render()
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(m_program);

        Transform model = RotationX(0);
        Transform view = m_camera.view();
        Transform projection = m_camera.projection(window_width(), window_height(), 45);

        //// . composer les transformations : model, view et projection
        Transform mvp = projection * view * model;
        Transform mv = projection * view;

        program_uniform(m_program, "mvpMatrix", mvp);
        program_uniform(m_program, "mvMatrix", mv);

        //// afficher chaque groupe
        const Materials& materials = m_world.materials();
        for (unsigned i = 0; i < m_groups.size(); i++)
        {
            const Material& material = materials(m_groups[i].index);

            //   couleur de la matiere du groupe de triangle
            program_uniform(m_program, "material_color", material.diffuse);

            //   texture definie par la matiere, ou texture neutre sinon...
            if (material.diffuse_texture != -1)
                program_use_texture(m_program, "material_texture", 0, m_textures[material.diffuse_texture]);
            else
                program_use_texture(m_program, "material_texture", 0, m_white_texture);

            // 1 draw par groupe de triangles...
            m_world.draw(m_groups[i].first, m_groups[i].n, m_program, /* use position */ true,
                /* use texcoord */ true, /* use normal */ true, /* use color */ false, /* use material index*/ false);
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
};


int main(int argc, char** argv)
{
    TP tp;
    tp.run();
    return 0;
}

