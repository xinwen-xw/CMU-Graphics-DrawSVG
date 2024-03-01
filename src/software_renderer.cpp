#include "software_renderer.h"

#include <cmath>
#include <vector>
#include <iostream>
#include <algorithm>

#include "triangulation.h"

using namespace std;

namespace CMU462 {


// Implements SoftwareRenderer //

void SoftwareRendererImp::draw_svg( SVG& svg ) {
  clear_sample();

  // set top level transformation
  transformation = svg_2_screen;

  // draw all elements
  for ( size_t i = 0; i < svg.elements.size(); ++i ) {
    draw_element(svg.elements[i]);
  }

  // draw canvas outline
  Vector2D a = transform(Vector2D(    0    ,     0    )); a.x--; a.y--;
  Vector2D b = transform(Vector2D(svg.width,     0    )); b.x++; b.y--;
  Vector2D c = transform(Vector2D(    0    ,svg.height)); c.x--; c.y++;
  Vector2D d = transform(Vector2D(svg.width,svg.height)); d.x++; d.y++;

  rasterize_line(a.x, a.y, b.x, b.y, Color::Black);
  rasterize_line(a.x, a.y, c.x, c.y, Color::Black);
  rasterize_line(d.x, d.y, b.x, b.y, Color::Black);
  rasterize_line(d.x, d.y, c.x, c.y, Color::Black);

  // resolve and send to render target
  resolve();

}

void SoftwareRendererImp::set_sample_rate( size_t sample_rate ) {

  // Task 4: 
  // You may want to modify this for supersampling support
  this->sample_rate = sample_rate;
  this->sample_buffer.resize(4 * (target_w * sample_rate) * (target_h * sample_rate));
}

void SoftwareRendererImp::set_render_target( unsigned char* render_target,
                                             size_t width, size_t height ) {

  // Task 4: 
  // You may want to modify this for supersampling support
  this->render_target = render_target;
  this->target_w = width;
  this->target_h = height;
  
  int supersampling_size = 4 * target_w * target_h;
  this->sample_buffer = std::vector<unsigned char>(supersampling_size, 255);
}

void SoftwareRendererImp::draw_element( SVGElement* element ) {

  // Task 5 (part 1):
  // Modify this to implement the transformation stack

  // Push transformation stack 
  Matrix3x3 transformation_save = transformation;

  // Update transformation
  transformation = transformation * element->transform;


  switch(element->type) {
    case POINT:
      draw_point(static_cast<Point&>(*element));
      break;
    case LINE:
      draw_line(static_cast<Line&>(*element));
      break;
    case POLYLINE:
      draw_polyline(static_cast<Polyline&>(*element));
      break;
    case RECT:
      draw_rect(static_cast<Rect&>(*element));
      break;
    case POLYGON:
      draw_polygon(static_cast<Polygon&>(*element));
      break;
    case ELLIPSE:
      draw_ellipse(static_cast<Ellipse&>(*element));
      break;
    case IMAGE:
      draw_image(static_cast<Image&>(*element));
      break;
    case GROUP:
      draw_group(static_cast<Group&>(*element));
      break;
    default:
      break;
  }

  // Pop current transformation of the current element
  transformation = transformation_save;
}


// Primitive Drawing //

void SoftwareRendererImp::draw_point( Point& point ) {

  Vector2D p = transform(point.position);
  rasterize_point( p.x, p.y, point.style.fillColor );

}

void SoftwareRendererImp::draw_line( Line& line ) { 

  Vector2D p0 = transform(line.from);
  Vector2D p1 = transform(line.to);
  rasterize_line( p0.x, p0.y, p1.x, p1.y, line.style.strokeColor );

}

void SoftwareRendererImp::draw_polyline( Polyline& polyline ) {

  Color c = polyline.style.strokeColor;

  if( c.a != 0 ) {
    int nPoints = polyline.points.size();
    for( int i = 0; i < nPoints - 1; i++ ) {
      Vector2D p0 = transform(polyline.points[(i+0) % nPoints]);
      Vector2D p1 = transform(polyline.points[(i+1) % nPoints]);
      rasterize_line( p0.x, p0.y, p1.x, p1.y, c );
    }
  }
}

void SoftwareRendererImp::draw_rect( Rect& rect ) {

  Color c;
  
  // draw as two triangles
  float x = rect.position.x;
  float y = rect.position.y;
  float w = rect.dimension.x;
  float h = rect.dimension.y;

  Vector2D p0 = transform(Vector2D(   x   ,   y   ));
  Vector2D p1 = transform(Vector2D( x + w ,   y   ));
  Vector2D p2 = transform(Vector2D(   x   , y + h ));
  Vector2D p3 = transform(Vector2D( x + w , y + h ));
  
  // draw fill
  c = rect.style.fillColor;
  if (c.a != 0 ) {
    rasterize_triangle( p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, c );
    rasterize_triangle( p2.x, p2.y, p1.x, p1.y, p3.x, p3.y, c );
  }

  // draw outline
  c = rect.style.strokeColor;
  if( c.a != 0 ) {
    rasterize_line( p0.x, p0.y, p1.x, p1.y, c );
    rasterize_line( p1.x, p1.y, p3.x, p3.y, c );
    rasterize_line( p3.x, p3.y, p2.x, p2.y, c );
    rasterize_line( p2.x, p2.y, p0.x, p0.y, c );
  }

}

void SoftwareRendererImp::draw_polygon( Polygon& polygon ) {

  Color c;

  // draw fill
  c = polygon.style.fillColor;
  if( c.a != 0 ) {

    // triangulate
    vector<Vector2D> triangles;
    triangulate( polygon, triangles );

    // draw as triangles
    for (size_t i = 0; i < triangles.size(); i += 3) {
      Vector2D p0 = transform(triangles[i + 0]);
      Vector2D p1 = transform(triangles[i + 1]);
      Vector2D p2 = transform(triangles[i + 2]);
      rasterize_triangle( p0.x, p0.y, p1.x, p1.y, p2.x, p2.y, c );
    }
  }

  // draw outline
  c = polygon.style.strokeColor;
  if( c.a != 0 ) {
    int nPoints = polygon.points.size();
    for( int i = 0; i < nPoints; i++ ) {
      Vector2D p0 = transform(polygon.points[(i+0) % nPoints]);
      Vector2D p1 = transform(polygon.points[(i+1) % nPoints]);
      rasterize_line( p0.x, p0.y, p1.x, p1.y, c );
    }
  }
}

void SoftwareRendererImp::draw_ellipse( Ellipse& ellipse ) {

  // Extra credit 

}

void SoftwareRendererImp::draw_image( Image& image ) {

  Vector2D p0 = transform(image.position);
  Vector2D p1 = transform(image.position + image.dimension);

  rasterize_image( p0.x, p0.y, p1.x, p1.y, image.tex );
}

void SoftwareRendererImp::draw_group( Group& group ) {

  for ( size_t i = 0; i < group.elements.size(); ++i ) {
    draw_element(group.elements[i]);
  }

}

void SoftwareRendererImp::fill_pixel(int x, int y, const Color& c ) {
  // check bounds
  if ( x < 0 || x >= target_w ) return;
  if ( y < 0 || y >= target_h ) return;

  // fill all samples for the pixel - NOT doing alpha blending!
  for ( int i = 0; i < sample_rate; ++i ) {
    for ( int j = 0; j < sample_rate; ++j ) {
      fill_sample(x, y, i, j, c);
    }
  }
}

void SoftwareRendererImp::fill_sample( int sx, int sy, int si, int sj, const Color& c ) {
  // check bounds
  if ( sx < 0 || sx >= target_w ) return;
  if ( sy < 0 || sy >= target_h ) return;

  int pixels = sx + sy * target_w;
  int samples = pixels * sample_rate * sample_rate + si + sj * sample_rate;
  sample_buffer[ 4 * samples ] = (uint8_t) (c.r * 255);
  sample_buffer[ 4 * samples + 1 ] = (uint8_t) (c.g * 255);
  sample_buffer[ 4 * samples + 2 ] = (uint8_t) (c.b * 255);
  sample_buffer[ 4 * samples + 3 ] = (uint8_t) (c.a * 255);
}

// Rasterization //

// The input arguments in the rasterization functions 
// below are all defined in screen space coordinates

void SoftwareRendererImp::rasterize_point( float x, float y, Color color ) {

  // fill in the nearest pixel
  int sx = (int) floor(x);
  int sy = (int) floor(y);

  // // check bounds
  // if ( sx < 0 || sx >= target_w ) return;
  // if ( sy < 0 || sy >= target_h ) return;

  // // fill sample - NOT doing alpha blending!
  // render_target[4 * (sx + sy * target_w)    ] = (uint8_t) (color.r * 255);
  // render_target[4 * (sx + sy * target_w) + 1] = (uint8_t) (color.g * 255);
  // render_target[4 * (sx + sy * target_w) + 2] = (uint8_t) (color.b * 255);
  // render_target[4 * (sx + sy * target_w) + 3] = (uint8_t) (color.a * 255);
  fill_pixel(sx, sy, color);
}

void SoftwareRendererImp::rasterize_line( float x0, float y0,
                                          float x1, float y1,
                                          Color color) {

  // Task 2: 
  // Implement line rasterization
  float dx = x1 - x0,
        dy = y1 - y0;

  if ( dy == 0 ) {
    for (int x = (int) floor(std::min(x0, x1)); x < std::max(x0, x1); ++x) {
      rasterize_point(x, y0, color);
    }
  } else if ( dx == 0 ) {
    for (int y = (int) floor(std::min(y0, y1)); y < std::max(y0, y1); ++y) {
      rasterize_point(x0, y, color);
    } 
  } else {
    float m = dy / dx;
    if ( m > 0 ) {
      float stepStart, stepEnd, inc, other, esp = 0;
      if ( 0 < m && m <= 1 ) {
        stepStart = x0,
        stepEnd = x1;
        other = y0;
        inc = m;
      } else {
        stepStart = y0,
        stepEnd = y1;
        other = x0;
        inc = 1 / m;
      }
      if ( stepStart < stepEnd ) {
        for ( int step = (int) floor(stepStart); step < stepEnd; ++step) {
          if ( 0 < m && m <= 1 ) {
            rasterize_point(step, other, color);
          } else {
            rasterize_point(other, step, color);
          }
          esp += inc;
          if ( esp >= 0.5 ) {
            other += 1;
            esp -= 1;
          }
        }
      } else {
        rasterize_line(x1, y1, x0, y0, color);
      }
    } else {
      float stepStart, stepEnd, inc, other, esp = 0;
      if ( 0 > m && m >= -1 ) {
        stepStart = x0,
        stepEnd = x1;
        other = y0;
        inc = m;
      } else {
        stepStart = y0,
        stepEnd = y1;
        other = x0;
        inc = 1 / m;
      }
      if ( stepStart < stepEnd ){
        for ( int step = (int) floor(stepStart); step < stepEnd; ++step) {
          if ( 0 > m && m >= -1 ) { 
            rasterize_point(step, other, color);
          } else {
            rasterize_point(other, step, color);
          }
          esp += inc;
          if ( esp <= -0.5 ) {
            other -= 1;
            esp += 1;
          }
        }
      } else {
        rasterize_line(x1, y1, x0, y0, color);
      }
    }
  }
}

void SoftwareRendererImp::rasterize_triangle( float x0, float y0,
                                              float x1, float y1,
                                              float x2, float y2,
                                              Color color ) {
  // Task 3: 
  // Implement triangle rasterization
  int lowerBoundX = (int) floor(std::min({x0, x1, x2}));
  int upperBoundX = (int) ceil(std::max({x0, x1, x2}));
  int lowerBoundY = (int) floor(std::min({y0, y1, y2}));
  int upperBoundY = (int) ceil(std::max({y0, y1, y2}));

  for (int x = lowerBoundX; x < upperBoundX; ++x) {
    for (int y = lowerBoundY; y < upperBoundY; ++y) {
      for (int i = 0; i < sample_rate; i++) {
        for (int j = 0; j < sample_rate; j++) {
          float centerX = x + (1 / (2 * sample_rate)) * (i * 2 + 1);
          float centerY = y + (1 / (2 * sample_rate)) * (j * 2 + 1);
          
          // determine if point is in all three half planes
          float cross1 = (x1 - x0) * (centerY - y0) - (y1 - y0) * (centerX - x0); 
          float cross2 = (x2 - x1) * (centerY - y1) - (y2 - y1) * (centerX - x1);
          float cross3 = (x0 - x2) * (centerY - y2) - (y0 - y2) * (centerX - x2);

          if ( (cross1 >= 0 && cross2 >= 0 && cross3 >= 0) || (cross1 <= 0 && cross2 <= 0 && cross3 <=0 ) ) {
            fill_sample(x, y, i, j, color);      
          }
        }
      }
    }
  }
}

void SoftwareRendererImp::rasterize_image( float x0, float y0,
                                           float x1, float y1,
                                           Texture& tex ) {
  // Task 6: 
  // Implement image rasterization

}

// resolve samples to render target
void SoftwareRendererImp::resolve( void ) {

  // Task 4: 
  // Implement supersampling
  // You may also need to modify other functions marked with "Task 4".
  int sample_per_pixel = sample_rate * sample_rate;

  for (int x = 0; x < this->target_w; ++x) {
    for (int y = 0; y < this->target_h; ++y) {
      // iterate over rgba, average all samples for the pixel
      for (int c = 0; c < 4; ++c) {
        int sum = 0;
        for (int i = 0; i < sample_rate; ++i) {
          for (int j = 0; j < sample_rate; ++j) {
            int pixels = x + y * target_w;
            int samples = pixels * sample_rate * sample_rate + i + j * sample_rate;
            sum += sample_buffer[4 * samples + c];
          }
        }
        render_target[4 * (x + y * target_w) + c] = sum / (sample_per_pixel);
      }
    }
  }
    
}


} // namespace CMU462
