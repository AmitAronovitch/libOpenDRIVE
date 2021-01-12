#include "Road.h"
#include "RefLine.h"

#include <array>
#include <cmath>
#include <iterator>
#include <math.h>
#include <utility>

namespace odr
{
double Crossfall::get_crossfall(double s, bool on_left_side) const
{
    const Poly3 poly = this->get_poly(s);

    if (this->s_start_to_poly.size() > 0)
    {
        auto target_poly_iter = this->s_start_to_poly.upper_bound(s);
        if (target_poly_iter != this->s_start_to_poly.begin())
            target_poly_iter--;

        Side side = Side::Both; // applicable side of the road
        if (this->sides.find(target_poly_iter->first) != this->sides.end())
            side = this->sides.at(target_poly_iter->first);

        if (on_left_side && side == Side::Right)
            return 0;
        else if (!on_left_side && side == Side::Left)
            return 0;

        return target_poly_iter->second.get(s);
    }

    return 0;
}

ConstLaneSectionSet Road::get_lanesections() const
{
    ConstLaneSectionSet lanesections;
    for (const auto& s_lansection : this->s_to_lanesection)
        lanesections.insert(s_lansection.second);

    return lanesections;
}

LaneSectionSet Road::get_lanesections()
{
    LaneSectionSet lanesections;
    for (const auto& s_lansection : this->s_to_lanesection)
        lanesections.insert(s_lansection.second);

    return lanesections;
}

std::shared_ptr<const LaneSection> Road::get_lanesection(double s) const
{
    if (this->s_to_lanesection.size() > 0)
    {
        auto target_lane_sec_iter = this->s_to_lanesection.upper_bound(s);
        if (target_lane_sec_iter != this->s_to_lanesection.begin())
            target_lane_sec_iter--;
        return target_lane_sec_iter->second;
    }

    return nullptr;
}

std::shared_ptr<LaneSection> Road::get_lanesection(double s)
{
    std::shared_ptr<LaneSection> lanesection = std::const_pointer_cast<LaneSection>(static_cast<const Road&>(*this).get_lanesection(s));
    return lanesection;
}

Vec3D Road::get_xyz(double s, double t, double h) const
{
    const Mat3D trans_mat = this->get_transformation_matrix(s);
    const Vec3D xyz = MatVecMultiplication(trans_mat, Vec3D{t, h, 1});

    return xyz;
}

Mat3D Road::get_transformation_matrix(double s) const
{
    const Vec3D  s_vec = this->ref_line->get_grad(s);
    const double superelevation = this->superelevation.get(s);

    const Vec3D e_t = normalize(Vec3D{-s_vec[1], s_vec[0], std::tan(superelevation) * std::abs(s_vec[1])});
    const Vec3D e_h = normalize(crossProduct(s_vec, e_t));
    const Vec3D p0 = this->ref_line->get_xyz(s);

    const Mat3D trans_mat{{{e_t[0], e_h[0], p0[0]}, {e_t[1], e_h[1], p0[1]}, {e_t[2], e_h[2], p0[2]}}};

    return trans_mat;
}

Vec3D Road::get_surface_pt(double s, double t) const
{
    std::shared_ptr<const LaneSection> lanesection = this->get_lanesection(s);
    if (!lanesection)
    {
        printf("road #%s - could not get lane section for s: %.2f\n", this->id.c_str(), s);
        return this->get_xyz(s, t, 0.0);
    }

    std::shared_ptr<const Lane> lane = lanesection->get_lane(s, t);

    const double t_inner_brdr = lane->inner_border.get(s);
    double       h_t = 0;

    if (lane->level)
    {
        const double h_inner_brdr = -std::tan(this->crossfall.get_crossfall(s, (lane->id > 0))) * std::abs(t_inner_brdr);
        const double superelev = this->superelevation.get(s); // cancel out superelevation
        h_t = h_inner_brdr + std::tan(superelev) * (t - t_inner_brdr);
    }
    else
    {
        h_t = -std::tan(this->crossfall.get_crossfall(s, (lane->id > 0))) * std::abs(t);
    }

    if (lane->s_to_height_offset.size() > 0)
    {
        const std::map<double, HeightOffset>& height_offs = lane->s_to_height_offset;

        auto s0_height_offs_iter = height_offs.upper_bound(s);
        if (s0_height_offs_iter != height_offs.begin())
            s0_height_offs_iter--;

        const double t_outer_brdr = lane->outer_border.get(s);
        const double inner_height = s0_height_offs_iter->second.inner;
        const double outer_height = s0_height_offs_iter->second.outer;
        const double p_t = (t_outer_brdr != t_inner_brdr) ? (t - t_inner_brdr) / (t_outer_brdr - t_inner_brdr) : 0.0;
        h_t += p_t * (outer_height - inner_height) + inner_height;

        if (std::next(s0_height_offs_iter) != height_offs.end())
        {
            /* if successive lane height entry available linearly interpolate */
            const double ds = std::next(s0_height_offs_iter)->first - s0_height_offs_iter->first;
            const double d_lh_inner = std::next(s0_height_offs_iter)->second.inner - inner_height;
            const double dh_inner = (d_lh_inner / ds) * (s - s0_height_offs_iter->first);
            const double d_lh_outer = std::next(s0_height_offs_iter)->second.outer - outer_height;
            const double dh_outer = (d_lh_outer / ds) * (s - s0_height_offs_iter->first);

            h_t += p_t * (dh_outer - dh_inner) + dh_inner;
        }
    }

    return this->get_xyz(s, t, h_t);
}

} // namespace odr