/*!
 * @file
 * @brief This file contains implementation of gpu
 *
 * @author Tomáš Milet, imilet@fit.vutbr.cz
 */

#include <student/gpu.hpp>
#include <iostream>


//! [drawTrianglesImpl]
struct Triangle {
  OutVertex points[3];
};

struct Point {
  float x,y,z;
};

uint32_t computeVertexID(VertexArray const&vao, uint32_t shaderInvocation) {
  if(!vao.indexBuffer) return shaderInvocation;

  if (vao.indexType == IndexType::UINT32) {
    uint32_t*ind = (uint32_t*)vao.indexBuffer;
    return (uint32_t) ind[shaderInvocation];
  } else if (vao.indexType == IndexType::UINT16) {
    uint16_t*ind = (uint16_t*)vao.indexBuffer;
    return (uint32_t) ind[shaderInvocation];
  } else {
    uint8_t*ind = (uint8_t*)vao.indexBuffer;
    return (uint32_t) ind[shaderInvocation];
  }
}

void loadVertex(InVertex&inVertex, VertexArray vao, uint32_t ID, bool userData, uint8_t* n_data) {
  uint64_t offset;
  uint64_t stride;
  AttributeType type;
  uint8_t* data;

  inVertex.gl_VertexID = computeVertexID(vao, ID);

    for (int j = 0; j < maxAttributes; j++) {
      type = vao.vertexAttrib[j].type;
      offset = vao.vertexAttrib[j].offset;
      stride = vao.vertexAttrib[j].stride;
      if (userData)
        data = (uint8_t*) vao.vertexAttrib[j].bufferData;
      else
        data = (uint8_t*) n_data;

      switch (type) {
        case AttributeType::FLOAT:
          inVertex.attributes[j].v1 = (float) ((float*) (data+offset+inVertex.gl_VertexID*stride))[0];
          break;
        case AttributeType::VEC2:
          inVertex.attributes[j].v2 = (glm::vec2) ((glm::vec2*) (data+offset+inVertex.gl_VertexID*stride))[0];
          break;
        case AttributeType::VEC3:
          inVertex.attributes[j].v3 = (glm::vec3) ((glm::vec3*) (data+offset+inVertex.gl_VertexID*stride))[0];
          break;
        case AttributeType::VEC4:
          inVertex.attributes[j].v4 = (glm::vec4) ((glm::vec4*) (data+offset+inVertex.gl_VertexID*stride))[0];
          break;
        default:
          continue;
      }
    }
  return;
}

void loadTriangle(Triangle &triangle,Program const&ptr,VertexArray const&vao,uint32_t tId, bool userData, uint8_t* data) {
  for(int i = 0; i < 3; i++) {
    InVertex inVertex;
    loadVertex(inVertex,vao,tId*3+i, userData, data);
    ptr.vertexShader(triangle.points[i], inVertex, ptr.uniforms);
  }
}

void perspectiveDivision(Triangle&triangle) {
  float xpos, ypos, zpos, wpos;
  for (int i = 0; i < 3; i++) {
    xpos = triangle.points[i].gl_Position.x;
    ypos = triangle.points[i].gl_Position.y;
    zpos = triangle.points[i].gl_Position.z;
    wpos = triangle.points[i].gl_Position.w;
    
    triangle.points[i].gl_Position.x = xpos / wpos;
    triangle.points[i].gl_Position.y = ypos / wpos;
    triangle.points[i].gl_Position.z = zpos / wpos;
  }
}

void viewportTransformation(Triangle&triangle, uint32_t f_width, uint32_t f_height) {
  float xpos, ypos;
  for (int i = 0; i < 3; i++) {
    xpos = triangle.points[i].gl_Position.x;
    ypos = triangle.points[i].gl_Position.y;

    triangle.points[i].gl_Position.x = f_width*((xpos+1)/2);
    triangle.points[i].gl_Position.y = f_height*((ypos+1)/2);
  }
  return;
}

bool isPixelInTriangle(Triangle triangle, float pixel_x, float pixel_y, float&lambda, float&lambda_x, float&lambda_y, float&lambda_z) {
  Point vec = { .x = pixel_x + 0.5f, .y = pixel_y + 0.5f };
  Point a = { .x = triangle.points[0].gl_Position.x, .y = triangle.points[0].gl_Position.y , .z = triangle.points[0].gl_Position.z };
  Point b = { .x = triangle.points[1].gl_Position.x, .y = triangle.points[1].gl_Position.y , .z = triangle.points[1].gl_Position.z };
  Point c = { .x = triangle.points[2].gl_Position.x, .y = triangle.points[2].gl_Position.y , .z = triangle.points[2].gl_Position.z };

  float T = 1.0f / ((b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y));
  lambda_x = ((b.y - c.y) * (vec.x - c.x) + (c.x - b.x) * (vec.y - c.y)) * T;
  lambda_y = ((c.y - a.y) * (vec.x - c.x) + (a.x - c.x) * (vec.y - c.y)) * T;
  lambda_z = 1.0f - lambda_x - lambda_y;
  lambda = a.z * lambda_x + b.z * lambda_y + c.z * lambda_z;

  if (lambda_x > 0 && lambda_y > 0 && lambda_z > 0) {
    return true;
  }
  return false;
}

bool setFragmentPosition(float&fragment_x_set, float&fragment_y_set, float&fragment_z_set, int frame_x, int frame_y, Frame frame, Triangle triangle, float lambda) {
  float fragment_x = frame_x + 0.5f;
  float fragment_y = frame_y + 0.5f;
  float fragment_z = lambda;
  float width = (float) frame.width;
  float height = (float) frame.height;

  if (fragment_x >= width || fragment_x <= 0 || fragment_y >= height || fragment_y <= 0) {
    return false;
  }
  fragment_x_set = fragment_x;
  fragment_y_set = fragment_y;
  fragment_z_set = fragment_z;

  return true;
}

void setFragmentDepth(glm::vec3&fragment_color, Triangle triangle, float lambda, float lambda_x, float lambda_y, float lambda_z, Frame&frame, int x, int y) {
  glm::vec3 A_color = triangle.points[0].attributes[0].v3;
  glm::vec3 B_color = triangle.points[1].attributes[0].v3;
  glm::vec3 C_color = triangle.points[2].attributes[0].v3;
  float h0 = triangle.points[0].gl_Position.w;
  float h1 = triangle.points[1].gl_Position.w;
  float h2 = triangle.points[2].gl_Position.w;

  fragment_color = ((A_color * lambda_x) / h0 + (B_color * lambda_y) / h1 + (C_color * lambda_z) / h2) / (lambda_x / h0 + lambda_y / h1 + lambda_z / h2);
}

void clampColor(OutFragment&outFragment, float min, float max) {
  outFragment.gl_FragColor = glm::clamp(glm::vec4(outFragment.gl_FragColor), min, max);
  outFragment.gl_FragColor *= 255.f;
}

void setFrameColor(Frame&frame, int* position, uint8_t R, uint8_t G, uint8_t B, uint8_t A) {
  uint8_t fr_x = position[0];
  uint8_t fr_y = position[1];
  frame.color[(fr_y*frame.width+fr_x)*4] = R;
  frame.color[(fr_y*frame.width+fr_x)*4+1] = G;
  frame.color[(fr_y*frame.width+fr_x)*4+2] = B;
  frame.color[(fr_y*frame.width+fr_x)*4+3] = A;
}

void setFrameDepth(Frame&frame, int* position, uint8_t D) {
  uint8_t fr_x = position[0];
  uint8_t fr_y = position[1];
  frame.depth[(fr_y*frame.width+fr_x)] = 255;
}

void getFrameColor(Frame&frame, int* position, uint8_t*color) {
  uint8_t fr_x = position[0];
  uint8_t fr_y = position[1];
  color[0] = frame.color[(fr_y*frame.width+fr_x)*4];
  color[1] = frame.color[(fr_y*frame.width+fr_x)*4+1];
  color[2] = frame.color[(fr_y*frame.width+fr_x)*4+2];
  color[3] = frame.color[(fr_y*frame.width+fr_x)*4+3];
}

void getFrameDepth(Frame&frame, int* position, float&depth) {
  uint8_t fr_x = position[0];
  uint8_t fr_y = position[1];
  depth = frame.depth[(fr_y*frame.width+fr_x)];
}

void depthTest(Frame&frame, int* position, InFragment inFr, OutFragment outFr) {
  uint8_t color[4];
  float buffer_depth;
  glm::vec4 newfr_color = outFr.gl_FragColor;
  float frag_depth = inFr.gl_FragCoord.z;
  getFrameColor(frame, position, color);
  getFrameDepth(frame, position, buffer_depth);
  if (buffer_depth > frag_depth) {
    setFrameColor(frame, position, newfr_color.r, newfr_color.g, newfr_color.b, newfr_color.a);
    setFrameDepth(frame, position, frag_depth);
  }
}

void rasterize(Frame&frame,Triangle &triangle,Program const&prg, uint32_t tID, VertexArray&vao) {
  bool fragPosres;
  float lambda, lambda_x, lambda_y, lambda_z;
  int frame_pos[2] = { 0 };
  for(int fr_y = 0; fr_y < frame.height; fr_y++) {
    for(int fr_x = 0; fr_x < frame.width; fr_x++){
      if(isPixelInTriangle(triangle, fr_x, fr_y, lambda, lambda_x, lambda_y, lambda_z)){
        frame_pos[0] = fr_x;
        frame_pos[1] = fr_y;
        InFragment inFragment;
        OutFragment outFragment;
        fragPosres = setFragmentPosition(inFragment.gl_FragCoord.x, inFragment.gl_FragCoord.y, inFragment.gl_FragCoord.z ,fr_x, fr_y, frame, triangle, lambda);
        setFragmentDepth(inFragment.attributes->v3, triangle, lambda, lambda_x, lambda_y, lambda_z, frame, fr_x, fr_y);
        setFrameColor(frame, frame_pos, outFragment.gl_FragColor.r, outFragment.gl_FragColor.g, outFragment.gl_FragColor.b, outFragment.gl_FragColor.a);
        setFrameDepth(frame, frame_pos, inFragment.gl_FragCoord.z);
        if (fragPosres) {
          prg.fragmentShader(outFragment,inFragment, prg.uniforms);
        }
        clampColor(outFragment, 0, 1);
        depthTest(frame, frame_pos, inFragment, outFragment);
      }
    }
  }
}

void drawTrianglesImpl(GPUContext &ctx,uint32_t nofVertices){
  uint64_t offset;
  uint64_t stride;
  AttributeType type;
  uint8_t* data;

  for (int t = 0; t < nofVertices/3; t++) {
    Triangle triangle;
    loadTriangle(triangle, ctx.prg, ctx.vao, t, true, NULL);
    perspectiveDivision(triangle);
    viewportTransformation(triangle, ctx.frame.width, ctx.frame.height);
    rasterize(ctx.frame, triangle, ctx.prg, t, ctx.vao);
  }
}
//! [drawTrianglesImpl]

/**
 * @brief This function reads color from texture.
 *
 * @param texture texture
 * @param uv uv coordinates
 *
 * @return color 4 floats
 */
glm::vec4 read_texture(Texture const&texture,glm::vec2 uv){
  if(!texture.data)return glm::vec4(0.f);
  auto uv1 = glm::fract(uv);
  auto uv2 = uv1*glm::vec2(texture.width-1,texture.height-1)+0.5f;
  auto pix = glm::uvec2(uv2);
  //auto t   = glm::fract(uv2);
  glm::vec4 color = glm::vec4(0.f,0.f,0.f,1.f);
  for(uint32_t c=0;c<texture.channels;++c)
    color[c] = texture.data[(pix.y*texture.width+pix.x)*texture.channels+c]/255.f;
  return color;
}

/**
 * @brief This function clears framebuffer.
 *
 * @param ctx GPUContext
 * @param r red channel
 * @param g green channel
 * @param b blue channel
 * @param a alpha channel
 */
void clear(GPUContext&ctx,float r,float g,float b,float a){
  auto&frame = ctx.frame;
  auto const nofPixels = frame.width * frame.height;
  for(size_t i=0;i<nofPixels;++i){
    frame.depth[i] = 10e10f;
    frame.color[i*4+0] = static_cast<uint8_t>(glm::min(r*255.f,255.f));
    frame.color[i*4+1] = static_cast<uint8_t>(glm::min(g*255.f,255.f));
    frame.color[i*4+2] = static_cast<uint8_t>(glm::min(b*255.f,255.f));
    frame.color[i*4+3] = static_cast<uint8_t>(glm::min(a*255.f,255.f));
  }
}

