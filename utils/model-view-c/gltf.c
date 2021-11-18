#include "gltf.h"

#include "common.h"

#include "glad.h"
#include "SDL_image.h"
#include <assert.h>

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "shader_min.h"

typedef struct Shader_ {
   GLuint program;
   /* Attriutes. */
   GLuint vertex;
   GLuint vertex_normal;
   GLuint vertex_tex0;
   /* Vertex Uniforms. */
   GLuint Hmodel;
   GLuint Hprojection;
   /* Fragment uniforms. */
   GLuint baseColour_tex;
   GLuint metallic_tex;
   GLuint metallicFactor;
   GLuint roughnessFactor;
   GLuint baseColour;
   GLuint clearcoat;
   GLuint clearcoat_roughness;
} Shader;
static Shader object_shader;
static GLuint tex_zero = -1;
static GLuint tex_ones = -1;

typedef struct Material_ {
   char *name;       /**< Name of the material if applicable. */
   /* pbr_metallic_roughness */
   GLuint baseColour_tex;
   GLuint metallic_tex;
   GLfloat metallicFactor;
   GLfloat roughnessFactor;
   GLfloat baseColour[4];
   /* pbr_specular_glossiness */
   /* TODO */
   /* clearcoat */
   /*GLuint clearcoat_tex;
   GLuint clearcoat_roughness_tex;
   GLuint clearcoat_normal_tex; */
   GLfloat clearcoat;
   GLfloat clearcoat_roughness;
   /* misc. */
   GLuint normal_tex;
   GLuint occlusion_tex;
   GLuint emissive_tex;
   GLfloat emissiveFactor[3];
   //GLuint tex0;
} Material;

typedef struct Mesh_ {
   size_t nidx;      /**< Number of indices. */
   GLuint vbo_idx;   /**< Index VBO. */
   GLuint vbo_pos;   /**< Position VBO. */
   GLuint vbo_nor;   /**< Normal VBO. */
   GLuint vbo_tex;   /**< Texture coordinate VBO. */
   int material;     /**< ID of material to use. */
} Mesh;

struct Node_;
typedef struct Node_ {
   char *name;       /**< Name information. */
   GLfloat H[16];    /**< Homogeneous transform. */
   Mesh *mesh;       /**< Meshes. */
   size_t nmesh;     /**< Number of meshes. */
   struct Node_ *children; /**< Children mesh. */
   size_t nchildren; /**< Number of children mesh. */
} Node;

struct Object_ {
   Node *nodes;         /**< Nodes the object has. */
   size_t nnodes;       /**< Number of nodes. */
   Material *materials; /**< Available materials. */
   int nmaterials;      /**< Number of materials. */
   GLfloat radius;      /**< Sphere fit on the model centered at 0,0. */
};

static GLuint object_loadTexture( const cgltf_texture_view *ctex )
{
   GLuint tex;
   SDL_Surface *surface;

   /* Must haev texture to load it. */
   if ((ctex==NULL) || (ctex->texture==NULL))
      return 0;

   glGenTextures( 1, &tex );
   glBindTexture( GL_TEXTURE_2D, tex );

   surface = IMG_Load( ctex->texture->image->uri );
   if (surface==NULL)
      return 0;
   SDL_LockSurface( surface );
   glPixelStorei( GL_UNPACK_ALIGNMENT, MIN( surface->pitch&-surface->pitch, 8 ) );
   glTexImage2D( GL_TEXTURE_2D, 0, GL_SRGB_ALPHA,
         surface->w, surface->h, 0, surface->format->Amask ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, surface->pixels );
   SDL_UnlockSurface( surface );

   /* Set stuff. */
   if (ctex->texture->sampler != NULL) {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, ctex->texture->sampler->mag_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, ctex->texture->sampler->min_filter);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, ctex->texture->sampler->wrap_s);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, ctex->texture->sampler->wrap_t);
   }

   glBindTexture( GL_TEXTURE_2D, 0 );

   gl_checkErr();

   return tex;
}

/**
 * @brief Loads a material for the object.
 */
static int object_loadMaterial( Material *mat, const cgltf_material *cmat )
{
   const GLfloat white[4] = { 1., 1., 1., 1. };
   /* TODO complete this. */
   if (cmat->has_pbr_metallic_roughness) {
      memcpy( mat->baseColour, cmat->pbr_metallic_roughness.base_color_factor, sizeof(mat->baseColour) );
      mat->metallicFactor  = cmat->pbr_metallic_roughness.metallic_factor;
      mat->roughnessFactor = cmat->pbr_metallic_roughness.roughness_factor;
      mat->baseColour_tex  = object_loadTexture( &cmat->pbr_metallic_roughness.base_color_texture );
      mat->metallic_tex    = object_loadTexture( &cmat->pbr_metallic_roughness.metallic_roughness_texture );
   }
   else {
      memcpy( mat->baseColour, white, sizeof(mat->baseColour) );
      mat->metallicFactor  = 0.;
      mat->roughnessFactor = 0.5;
      mat->baseColour_tex  = tex_ones;
      mat->metallic_tex    = tex_ones;
   }

   if (cmat->has_clearcoat) {
      mat->clearcoat = cmat->clearcoat.clearcoat_factor;
      mat->clearcoat_roughness = cmat->clearcoat.clearcoat_roughness_factor;
   }
   else {
      mat->clearcoat = 0.;
      mat->clearcoat_roughness = 0.;
   }
   return 0;
}

/**
 * @brief Loads a VBO from an accessor.
 */
static GLuint object_loadVBO( cgltf_accessor *acc )
{
   GLuint vid;
   cgltf_size num = cgltf_accessor_unpack_floats( acc, NULL, 0 );
   cgltf_float *dat = malloc( sizeof(cgltf_float) * num );
   cgltf_accessor_unpack_floats( acc, dat, num );

   /* OpenGL magic. */
   glGenBuffers( 1, &vid );
   glBindBuffer( GL_ARRAY_BUFFER, vid );
   glBufferData( GL_ARRAY_BUFFER, sizeof(cgltf_float) * num, dat, GL_STATIC_DRAW );
   glBindBuffer( GL_ARRAY_BUFFER, 0 );

   gl_checkErr();
   free( dat );
   return vid;
}

/**
 * @brief Loads a mesh for the object.
 */
static int object_loadNodeRecursive( cgltf_data *data, Node *node, const cgltf_node *cnode )
{
   cgltf_mesh *cmesh = cnode->mesh;
   /* Get transform for node. */
   cgltf_node_transform_local( cnode, node->H );

   if (cmesh == NULL) {
      node->nmesh = 0;
   }
   else {
      /* Load meshes. */
      node->mesh = calloc( cmesh->primitives_count, sizeof(Mesh) );
      node->nmesh = cmesh->primitives_count;
      for (size_t i=0; i<cmesh->primitives_count; i++) {
         Mesh *mesh = &node->mesh[i];
         cgltf_primitive *prim = &cmesh->primitives[i];
         cgltf_accessor *acc = prim->indices;
         cgltf_size num = cgltf_num_components(acc->type) * acc->count;
         GLuint *idx = malloc( sizeof(cgltf_uint) * num );
         for (size_t j=0; j<num; j++)
            cgltf_accessor_read_uint( acc, j, &idx[j], 1 );

         /* Check material. */
         if (prim->material != NULL)
            mesh->material = prim->material - data->materials;

         /* Store indices. */
         glGenBuffers( 1, &mesh->vbo_idx );
         glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mesh->vbo_idx );
         glBufferData( GL_ELEMENT_ARRAY_BUFFER, sizeof(cgltf_uint) * num, idx, GL_STATIC_DRAW );
         mesh->nidx = acc->count;
         glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
         gl_checkErr();

         for (size_t j=0; j<prim->attributes_count; j++) {
            cgltf_attribute *attr = &prim->attributes[j];
            switch (attr->type) {
               case cgltf_attribute_type_position:
                  mesh->vbo_pos = object_loadVBO( attr->data );
                  break;

               case cgltf_attribute_type_normal:
                  mesh->vbo_nor = object_loadVBO( attr->data );
                  break;

               case cgltf_attribute_type_texcoord:
                  mesh->vbo_tex = object_loadVBO( attr->data );
                  break;

               case cgltf_attribute_type_color:
               case cgltf_attribute_type_tangent:
               default:
                  break;
            }
         }
      }
   }

   /* Iterate over children. */
   node->children = calloc( cnode->children_count, sizeof(Node) );
   node->nchildren = cnode->children_count;
   for (size_t i=0; i<cnode->children_count; i++)
      object_loadNodeRecursive( data, &node->children[i], cnode->children[i] );
   return 0;
}

static void object_renderMesh( const Object *obj, const Mesh *mesh, const GLfloat H[16] )
{
   const Material *mat = &obj->materials[ mesh->material ];
   Shader *shd = &object_shader;

   /* Depth testing. */
   glEnable(GL_DEPTH_TEST);
   glDepthFunc(GL_LESS);
   
   glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, mesh->vbo_idx );

   /* TODO put everything in a single VBO */
   glBindBuffer( GL_ARRAY_BUFFER, mesh->vbo_pos );
   glVertexAttribPointer( shd->vertex, 4, GL_FLOAT, GL_FALSE, 0, NULL );
   glEnableVertexAttribArray( shd->vertex );
   if (mesh->vbo_nor) {
      glBindBuffer( GL_ARRAY_BUFFER, mesh->vbo_nor );
      glVertexAttribPointer( shd->vertex_normal, 3, GL_FLOAT, GL_FALSE, 0, NULL );
      glEnableVertexAttribArray( shd->vertex_normal );
   }
   if (mesh->vbo_tex) {
      glBindBuffer( GL_ARRAY_BUFFER, mesh->vbo_tex );
      glVertexAttribPointer( shd->vertex_tex0, 2, GL_FLOAT, GL_FALSE, 0, NULL );
      glEnableVertexAttribArray( shd->vertex_tex0 );
   }

   /* Texture. */
   glActiveTexture( GL_TEXTURE0 );
   gl_checkErr();
   glBindTexture( GL_TEXTURE_2D, mat->baseColour_tex );

   /* Set up shader. */
   glUseProgram( shd->program );
   const GLfloat sca = 0.1;
   const GLfloat Hprojection[16] = {
      sca, 0.0, 0.0, 0.0,
      0.0, sca, 0.0, 0.0,
      0.0, 0.0, sca, 0.0,
      0.0, 0.0, 0.0, 1.0 };
   glUniformMatrix4fv( shd->Hprojection, 1, GL_FALSE, Hprojection );
   glUniformMatrix4fv( shd->Hmodel, 1, GL_FALSE, H );
   if (shd->metallicFactor)
      glUniform1f( shd->metallicFactor, mat->metallicFactor );
   if (shd->roughnessFactor)
      glUniform1f( shd->roughnessFactor, mat->roughnessFactor );
   if (shd->baseColour)
      glUniform4f( shd->baseColour, mat->baseColour[0], mat->baseColour[1], mat->baseColour[2], mat->baseColour[3] );
   if (shd->clearcoat)
      glUniform1f( shd->clearcoat, mat->clearcoat );
   if (shd->clearcoat_roughness)
      glUniform1f( shd->clearcoat_roughness, mat->clearcoat_roughness );

   glDrawElements( GL_TRIANGLES, mesh->nidx, GL_UNSIGNED_INT, 0 );

   glUseProgram( 0 );

   glBindTexture( GL_TEXTURE_2D, 0 );

   glBindBuffer( GL_ARRAY_BUFFER, 0 );
   glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );

   glDisable(GL_DEPTH_TEST);
   
   gl_checkErr();
}

void object_renderNode( const Object *obj, const Node *node, GLfloat H[16] )
{
   /* Multiply matrices, can be animated so not caching. */
   /* TODO cache when not animated. */
   const GLfloat *pm = node->H;
   for (int i=0; i<4; i++) {
      float l0 = H[i * 4 + 0];
      float l1 = H[i * 4 + 1];
      float l2 = H[i * 4 + 2];

      float r0 = l0 * pm[0] + l1 * pm[4] + l2 * pm[8];
      float r1 = l0 * pm[1] + l1 * pm[5] + l2 * pm[9];
      float r2 = l0 * pm[2] + l1 * pm[6] + l2 * pm[10];

      H[i * 4 + 0] = r0;
      H[i * 4 + 1] = r1;
      H[i * 4 + 2] = r2;
   }
   H[12] += pm[12];
   H[13] += pm[13];
   H[14] += pm[14];

   /* Draw meshes. */
   for (size_t i=0; i<node->nmesh; i++)
      object_renderMesh( obj, &node->mesh[i], H );

   /* Draw children. */
   for (size_t i=0; i<node->nchildren; i++)
      object_renderNode( obj, &node->children[i], H );
   
   gl_checkErr();
}

void object_render( const Object *obj )
{
   GLfloat H[16] = { 1.0, 0.0, 0.0, 0.0,
                     0.0, 1.0, 0.0, 0.0,
                     0.0, 0.0, 1.0, 0.0,
                     0.0, 0.0, 0.0, 1.0 };
   for (size_t i=0; i<obj->nnodes; i++)
      object_renderNode( obj, &obj->nodes[i], H );
}

Object *object_loadFromFile( const char *filename )
{
   Object *obj;
   cgltf_result res;
   cgltf_data *data;
   cgltf_options opts;
   memset( &opts, 0, sizeof(opts) );

   obj = calloc( sizeof(Object), 1 );

   res = cgltf_parse_file( &opts, filename, &data );
   assert( res == cgltf_result_success );

#if DEBUGGING
   res = cgltf_validate( data );
   assert( res == cgltf_result_success );
#endif /* DEBUGGING */

   res = cgltf_validate( data );
   assert( res == cgltf_result_success );

   /* TODO load buffers properly from physfs. */
   res = cgltf_load_buffers( &opts, data, "./" );
   assert( res == cgltf_result_success );

   /* Load materials. */
   obj->materials = calloc( data->materials_count, sizeof(Material) );
   obj->nmaterials = data->materials_count;
   for (size_t i=0; i<data->materials_count; i++)
      object_loadMaterial( &obj->materials[i], &data->materials[i] );

   /* Load nodes. */
   cgltf_scene *scene = &data->scenes[0]; /* data->scene may be NULL */
   obj->nodes = calloc( scene->nodes_count, sizeof(Node) );
   obj->nnodes = scene->nodes_count;
   for (size_t i=0; i<scene->nodes_count; i++)
      object_loadNodeRecursive( data, &obj->nodes[i], scene->nodes[i] );

   cgltf_free(data);

   return obj;
}

void object_free( Object *obj )
{
   /* TODO properly free shit. */
   free( obj );
}

int object_init (void)
{
   const GLubyte data_zero[4] = { 0, 0, 0, 0 };
   const GLubyte data_ones[4] = { 255, 255, 255, 255 };
   object_shader.program = gl_program_vert_frag( "gltf.vert", "gltf_pbr.frag" );
   if (object_shader.program==0)
      return -1;
   object_shader.vertex        = glGetAttribLocation( object_shader.program, "vertex" );
   object_shader.vertex_normal = glGetAttribLocation( object_shader.program, "vertex_normal" );
   object_shader.vertex_tex0   = glGetAttribLocation( object_shader.program, "vertex_tex0" );
   object_shader.Hprojection   = glGetUniformLocation( object_shader.program, "projection ");
   object_shader.Hmodel        = glGetUniformLocation( object_shader.program, "model ");
   gl_checkErr();

   glGenTextures( 1, &tex_zero );
   glBindTexture( GL_TEXTURE_2D, tex_zero );
   glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data_zero );
   glGenTextures( 1, &tex_ones );
   glBindTexture( GL_TEXTURE_2D, tex_ones );
   glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data_ones );
   glBindTexture( GL_TEXTURE_2D, 0 );

   gl_checkErr();
   return 0;
}

void object_exit (void)
{
}
