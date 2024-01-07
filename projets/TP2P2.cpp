// Shadow Mapping


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



vec3 cross(const vec3& a, const vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Transform lookAt(const vec3& position, const vec3& target, const vec3& up) {
    vec3 forward = normalize({ target.x - position.x, target.y - position.y, target.z - position.z });
    vec3 right = normalize(cross(forward, up));
    vec3 newUp = cross(right, forward);

    Transform result(
        right.x, right.y, right.z, 0,
        newUp.x, newUp.y, newUp.z, 0,
        -forward.x, -forward.y, -forward.z, 0,
        0, 0, 0, 1
    );

    return result;
}

class TP : public AppCamera
{
public:
    // constructeur : donner les dimensions de l'image, et eventuellement la version d'openGL.
    TP() : AppCamera(520, 520) {}

    // creation des objets de l'application
    int init()
    {

        //m_world = read_mesh("bistro-small/export.obj");
       m_world = read_mesh("data/bistro/exterior.obj");

        Point pmin, pmax;
        m_world.bounds(pmin, pmax);
        m_program = read_program("shaders/reality.glsl");
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
        m_white_texture = read_texture(0, "data/white.png");  // !! utiliser une vraie image blanche...


        m_groups = m_world.groups(m_world.m_triangle_materials);


        // Create framebuffer object and depth map texture
        glGenFramebuffers(1, &depthMapFBO);
        glGenTextures(1, &depthMap);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Create a simple depth map rendering program
        depthMapProgram = read_program("shaders/depth_map.glsl");
        program_print_errors(depthMapProgram);


        glClearColor(0.2f, 0.2f, 0.2f, 1.f);        // couleur par defaut de la fenetre
        glClearDepth(1.f);                          // profondeur par defaut
        glDepthFunc(GL_LESS);                       // ztest, conserver l'intersection la plus proche de la camera
        glEnable(GL_DEPTH_TEST);                    // activer le ztest
        //glEnable(GL_CULL_FACE);
        renderSceneFromLight();
        return 0;
    }

    // destruction des objets de l'application
    int quit()
    {
        m_world.release();
        release_program(m_program);

        return 0;   // pas d'erreur
    }
    void renderSceneFromLight()
    {
        glUseProgram(depthMapProgram);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0, 1.0);

        // Create the orthographic projection matrix
        Transform lightProjection = Ortho(-150, 150, -150, 150, float(-150), float(150));
        Vector lightPos = Vector(0, 8, 8);
        Vector lightDir = normalize(Vector(0, -1, -1));
      /*  Transform r = RotationX(-90);
        Transform t2 = Translation(lightPos);
        Transform m = r * t2;*/

        // Create the view matrix
        Transform lightView = lookAt(lightPos, Point(lightPos)+Point(lightDir), Vector(0, 1, 0));

        // Calculate the light space matrix
        Transform model = RotationX(0);
        //lightSpaceMatrix = Viewport(1, 1)*lightProjection * lightView * Translation(lightPos) * model;
        lightSpaceMatrix = Viewport(1, 1) * lightProjection * lightView * Translation(lightPos)* model;
        // Pass lightSpaceMatrix to the shader as before
        
        program_uniform(depthMapProgram, "lightSpaceMatrix", lightSpaceMatrix  );


        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glViewport(0, 0, 1024, 1024);
        glClear(GL_DEPTH_BUFFER_BIT);

        for (int j = 0; j < m_groups.size(); ++j)
        {
            m_world.draw(m_groups[j].first, m_groups[j].n, depthMapProgram, true, false, false, false, false);
        }
        glDisable(GL_POLYGON_OFFSET_FILL);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);  // Unbind the framebuffer
    }


    // dessiner une nouvelle image
    int render()
    {
        if (key_state('r'))
        {
            // change de point de vue
            m_program = read_program("shaders/reality.glsl");
        }       
        // screenshot
        if (key_state('s'))
        {
            clear_key_state('s');
            static int id = 1;
            screenshot("camera", id++);
        }
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport(0, 0, 520, 520);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(m_program);

        Transform model = RotationX(0);
        Transform view = m_camera.view();
        Transform projection = m_camera.projection(window_width(), window_height(), 45);

        //// . composer les transformations : model, view et projection
        Transform mvp = projection * view * model;
        Transform mv = projection * view;

        program_uniform(m_program, "mvpMatrix", mvp);
        program_uniform(m_program, "lightSpaceMatrix", lightSpaceMatrix);
        program_uniform(m_program, "lightDir", normalize(Vector(0, -1, -1)));


        // Bind the depth map texture to a texture unit (e.g., GL_TEXTURE1)
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        program_uniform(m_program, "shadowMap", 1);  // Pass the texture unit to the shader


        glEnable(GL_DEPTH_CLAMP);

        //// afficher chaque groupe
        for (int j = 0; j < m_groups.size(); ++j) {
            {
                //if (isPointInsideFrostum(Point(bbms[i].bbox.centerX, bbms[i].bbox.centerY, bbms[i].bbox.centerZ), mvp)) 
                {
                    const Materials& materials = m_world.materials();
                    const Material& material = materials(j);
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
                    m_world.draw(m_groups[j].first, m_groups[j].n, m_program, /* use position */ true,
                        /* use texcoord */ true, /* use normal */ true, /* use color */ false, /* use material index*/ false);
                    
                }
            }
        }
        glDisable(GL_DEPTH_CLAMP);

        return 1;
    }

protected:
    Mesh m_world;
    std::vector<GLuint> m_textures;
    GLuint m_white_texture;
    std::vector<TriangleGroup> m_groups;
    GLuint m_program;
    Transform lightSpaceMatrix;
    GLuint depthMapFBO;
    GLuint depthMap;
    GLuint depthMapProgram;

};


int main(int argc, char** argv)
{
    TP tp;
    tp.run();
    return 0;
}

