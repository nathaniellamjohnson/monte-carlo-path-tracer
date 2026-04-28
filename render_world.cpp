#include "render_world.h"
#include "flat_shader.h"
#include "object.h"
#include "light.h"
#include "ray.h"

//#include <iostream>
//using namespace std;

extern bool disable_hierarchy;

Render_World::Render_World()
    :background_shader(0),ambient_intensity(0),enable_shadows(true),
    recursion_depth_limit(3), rng(42), samples_per_pixel(64),
    enable_caustics(false), photons_per_light(60000),
    max_photons_gathered(200), gather_radius(0.05)
{
    caustic_map.reset(new Caustic_Photon_Map());
    caustic_map->point_cloud.photons = &caustic_photons;
}

Render_World::~Render_World()
{
    delete background_shader;
    for(size_t i=0;i<objects.size();i++) delete objects[i];
    for(size_t i=0;i<lights.size();i++) delete lights[i];
}

// Find and return the Hit structure for the closest intersection.  Be careful
// to ensure that hit.dist>=small_t.
Hit Render_World::Closest_Intersection(const Ray& ray)
{
    Hit closest_hit;
    closest_hit = {nullptr, 0, 0};
    double min_t = std::numeric_limits<double>::max();

    // The dumb way to do it is to scan thru everything and then check for the closest intersection
    for (Object* obj : objects)
    {
        Hit obj_intersection_hit = obj->Intersection(ray , -1); // potential source of error, check against all parts 
        auto obj_ptr = obj_intersection_hit.object;
        double dist = obj_intersection_hit.dist;

        if (obj_ptr != nullptr && dist >= small_t && dist < min_t)
        {
            closest_hit = obj_intersection_hit;
            min_t = dist; 
        }
    }
    return closest_hit;
}

// set up the initial view ray and call
void Render_World::Render_Pixel(const ivec2& pixel_index)
{
    size_t spp = samples_per_pixel; // samples per pixel
    vec3 color (0.0, 0.0, 0.0);

    for (size_t i = 0; i < spp; i++)
    {
        // Ray class has vec3 endpoint & vec3 direction
        Ray ray;
    
        // Get ray endpoint via pixel world position
        // get direction via normalize(pix_pos - cam_pos)
        ray.endpoint = camera.World_Position(pixel_index);
        ray.direction = (ray.endpoint - camera.position).normalized();

        color += Cast_Ray(ray,this->recursion_depth_limit - 1);
    }

    color /= spp;

    camera.Set_Pixel(pixel_index,Pixel_Color(color));
}

void Render_World::Render()
{
    if(!disable_hierarchy)
        Initialize_Hierarchy(); //ignore this untill the last 2 test cases

    if (enable_caustics)
    {
        Build_Caustic_Photon_Map(this->photons_per_light);
    }

    for(int j=0;j<camera.number_pixels[1];j++)
        for(int i=0;i<camera.number_pixels[0];i++)
            Render_Pixel(ivec2(i,j));
}

// cast ray and return the color of the closest intersected surface point,
// or the background color if there is no object intersection
// NOTE: whitting / old ray tracer code
// Left in out of sentimentality, I guess
// vec3 Render_World::Cast_Ray(const Ray& ray,int recursion_depth)
// {
//     vec3 color;

//     // Hit object of the min dist hit object
//     Hit minimum_dist_hit = {};
//     minimum_dist_hit.object = nullptr;

//     // World has object arrays
//     for (Object* o : objects)
//     {
//         // Check for hit & store minimum dist instance 
//         Hit hit_info = o->Intersection(ray, -1); // potential source of error -> negative part in order to check against everything
//         if (hit_info.object != NULL && ( minimum_dist_hit.object == nullptr || hit_info.dist < minimum_dist_hit.dist ))
//         {
//             minimum_dist_hit = hit_info;
//         }
//     }
    
//     // Check for hit at all 
//     if (minimum_dist_hit.object == nullptr)
//     {
//         // No intersections, background color 
//         color = background_shader->Shade_Surface(ray, vec3(0, 0, 0), vec3(0, 0, 0), recursion_depth); // potential source of error, what intersection point / normal to use?
//     }
//     else 
//     {
//         // Get color of that object via that object's shader
//         // fill & return that color
//         vec3 intersection_point = ray.endpoint + ray.direction * minimum_dist_hit.dist; // calculated via ray endpoint + ray.dir * dist 
//         color = minimum_dist_hit.object->material_shader->Shade_Surface(ray, intersection_point, minimum_dist_hit.object->Normal(intersection_point, minimum_dist_hit.part), recursion_depth);
//     }

//     return color;
// }

// vec3 Render_World::Cast_Ray(const Ray& ray,int recursion_depth) -> Monte Carlo
vec3 Render_World::Cast_Ray(const Ray& ray, int recursion_depth)
{
    // TODO: add russian roulette after recursion_depth - 2 - std::clamp(p, 0.1f, 0.9f);
    // TODO: subpixel jitter? probably in the caller 
    // todo: next event estimation, cornell box is super dark and sampling the light at each bound 
    //      would help brighten it up

    Hit hit = Closest_Intersection(ray);

    if (! hit.object)
    {
        if (background_shader)
        {
            return background_shader->Emission();
        }
        else
        {
            return vec3(0.0, 0.0, 0.0);
        }
    }

    const Object* obj = hit.object;
    Shader* mat = obj->material_shader;

    vec3 intersection_point = ray.endpoint + ray.direction * hit.dist;
    vec3 normal_at_intersection_point = obj->Normal(intersection_point, hit.part).normalized();
    vec3 wo = -ray.direction; // pointing towards camera    

    // flip normal if inside / outside 
    //if(dot(normal_at_intersection_point, wo) < 0)
    //{
    //    normal_at_intersection_point = -normal_at_intersection_point;
    //}

    vec3 caustic_irradiance(0.0, 0.0, 0.0);
    if (enable_caustics)
    {
        caustic_irradiance = Estimate_Caustic_Irradiance(*this, mat,
            intersection_point, normal_at_intersection_point);
    }

    // Emission
    vec3 Le = mat->Emission(); 

    // Stop bouncing, but still keep terminal emissive contribution.
    if (recursion_depth <= 0)
    {
        return Le + caustic_irradiance;
    }
    
    // BSDF Sample
    BSDF_Sample s = mat->Sample(normal_at_intersection_point, wo, rng);

    if (s.pdf <= 0.0)
    {
        // this direction is impossible (or LESS than impossible!)
        return Le + caustic_irradiance;
    }

    Ray new_ray; 
    new_ray.endpoint = intersection_point + s.direction * small_t;
    new_ray.direction = s.direction.normalized();

    vec3 Li = Cast_Ray(new_ray, recursion_depth - 1);

    float cosTheta = abs(dot(normal_at_intersection_point, s.direction));
 
    vec3 Lo = Le + caustic_irradiance + s.brdf * Li * (cosTheta / s.pdf);

    return Lo;
}

void Render_World::Initialize_Hierarchy()
{
    // TODO; // Fill in hierarchy.entries; there should be one entry for
    // each part of each object.

    hierarchy.Reorder_Entries();
    hierarchy.Build_Tree(); 
}

// Caustic Photon Mapping
void Render_World::Build_Caustic_Photon_Map(int photons_per_light, int max_bounces)
{
    caustic_photons.clear();
    caustic_map->kdtree.reset();

    if (photons_per_light <= 0 || max_bounces <= 0 || lights.empty())
    {
        return;
    }

    std::uniform_real_distribution<double> dist(0.0, 1.0);

    // WHY?
    caustic_photons.reserve((size_t)photons_per_light*lights.size()/4 + 1); //

    for (Light * light : lights)
    {
        vec3 photon_power = (light->color*light->brightness)/(double)photons_per_light;

        for (int i = 0; i < photons_per_light; i++)
        {
            Ray photon_ray(light->position, Random_Unit_Vector(rng));
            vec3 throughput = photon_power; 
            bool along_specular_path = false; 

            for (int bounce = 0; bounce < max_bounces; bounce++)
            {
                // TODO: do it again further along it we hit a flat_shader + is_emissive
                Hit hit = Closest_Intersection(photon_ray);

                if(hit.object==nullptr)
                {
                    break;
                }

                vec3 hit_point = photon_ray.Point(hit.dist);
                vec3 normal = hit.object->Normal(hit_point, hit.part).normalized();
                Shader* shader = hit.object->material_shader;

                if (shader == nullptr)
                {
                    break;
                }

                // If we hit a diffuse surface, we only record the photon 
                // if we were redirected by glass or mirrors
                if(!Is_Caustic_Specular(shader))
                {
                    if(along_specular_path)
                    {
                        Photon p;
                        p.position = hit_point;
                        p.direction = -1.0*photon_ray.direction;
                        p.power = throughput;
                        caustic_photons.push_back(p);
                    }
                    break;
                }

                along_specular_path = true;

                // AI Usage: ChatGPT suggested way to determine if the shader was reflective & set var (Apr 20th, 2026)
                if(const Reflective_Shader* reflective = dynamic_cast<const Reflective_Shader*>(shader))
                {
                    vec3 ray_dir = photon_ray.direction;
                    vec3 reflection_dir = (ray_dir - (2.0 * dot(ray_dir, normal) * normal)).normalized();
                    photon_ray = Ray(hit_point + reflection_dir*small_t,reflection_dir);
                    throughput *= reflective->reflectivity;
                } 
                // AI Usage: ChatGPT suggested way to determine if the shader was glass & set var (Apr 20th, 2026)
                else if(const Glass_Shader* glass = dynamic_cast<const Glass_Shader*>(shader))
                {
                    // normal is called normal
                    vec3 view = (-1.0*photon_ray.direction).normalized();
                    double eta_i = 1.0; // index of refraction for "incident medium"/air
                    double eta_t = std::max(1.0001,glass->ior); // index of refraction for "transmitted medium"/glass

                    double cos_theta = dot(normal,view);
                    if(cos_theta<0)
                    {
                        normal = -1.0*normal;
                        cos_theta = -cos_theta;
                        std::swap(eta_i,eta_t);
                    }

                    // Schlick approx of Fresnel to decide between reflect & refract
                    // basically, if perpendicular we should be more likely to refract than reflect
                    // & vice versa - if grazing, we should be more likely to reflect over refract
                    double r0 = (eta_i-eta_t)/(eta_i+eta_t);
                    r0 *= r0;
                    double fresnel = r0 + (1.0-r0)*pow(1.0-cos_theta,5.0);

                    // snells law to determine
                    double eta = eta_i/eta_t;
                    double k = 1.0 - eta*eta*(1.0-cos_theta*cos_theta); 

                    // choose between reflect & refract
                    vec3 next_dir;
                    if(k<=0 || dist(rng)<fresnel)
                    {
                        vec3 ray_dir = photon_ray.direction;
                        next_dir = (ray_dir - (2.0 * dot(ray_dir, normal) * normal)).normalized();
                        throughput *= fresnel;
                    }
                    else
                    {
                        next_dir = (eta*(-1.0*view) + (eta*cos_theta-std::sqrt(k))*normal).normalized();
                        throughput *= (1.0-fresnel);
                        throughput *= glass->color;
                    }

                    photon_ray = Ray(hit_point + next_dir*small_t,next_dir);
                }

                if(throughput.magnitude_squared()<1e-8)
                {
                    break; // early exit if we have low power
                }

            }
        }
        if(caustic_photons.empty())
        {
            return;
        }

        caustic_map->kdtree.reset(new Caustic_Photon_Map::KD_Tree(3,caustic_map->point_cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10)));
        caustic_map->kdtree->buildIndex();
    }
}

size_t Render_World::Query_Caustic_Photons(const vec3& position,double radius, size_t max_results,std::vector<size_t>& out_indices) const
{
    out_indices.clear();

    if(!caustic_map->kdtree || radius<=0 || max_results==0) // guards
    {
        return 0;
    } 

    std::vector<nanoflann::ResultItem<size_t, double>> matches; 
    matches.reserve(max_results);

    double query_pt[3] = {position[0],position[1], position[2]};
    nanoflann::SearchParameters params; 
    
    const size_t match_count = caustic_map->kdtree->radiusSearch(query_pt, radius*radius, matches, params);

    size_t count = std::min(max_results, match_count);
    out_indices.reserve(count);
    for (size_t i = 0; i < count; i++)
    {
        out_indices.push_back(matches[i].first);
    }

    return out_indices.size();
}

bool Render_World::Has_Caustic_Photon_Map() const
{
    return caustic_map->kdtree.get() != 0 && !caustic_photons.empty();
}

vec3 Render_World::Estimate_Caustic_Irradiance(const Render_World& world,const Shader* receiver_shader,const vec3& position,const vec3& normal)
{
    if(Is_Caustic_Specular(receiver_shader))
    {
        return vec3();
    } 
    if(!world.Has_Caustic_Photon_Map())
    {
        return vec3();
    }
    if(world.gather_radius<=0 || world.max_photons_gathered<=0)
    {
        return vec3();
    }

    const double radius = world.gather_radius;
    const size_t max_photons = (size_t)world.max_photons_gathered;

    std::vector<size_t> photon_indices;
    const size_t found = world.Query_Caustic_Photons(position,radius,max_photons,photon_indices);
    if(found==0)
    {
        return vec3();
    }

    vec3 flux;
    vec3 n = normal.normalized();
    for(size_t idx : photon_indices)
    {
        const Photon& photon = world.caustic_photons[idx];
        double weight = std::max(0.0,dot(n,photon.direction));
        flux += photon.power*weight;
    }

    // Photon density estimate over disk area.
    const double area = pi*radius*radius;
    const double caustic_intensity_scale = 0.85;
    return flux*(caustic_intensity_scale/area);
}
