# Render splines using Cycles hair primitives

![](https://www.antillevisuals.com/technical-research/cycles_mod_splines_as_hair.jpg)

## Features
This is a Cycles modification that allows you to skip meshing splines entirely and render them as hair. Needless to say, with this method you can render *lots* of them (hello large-scale).
Splines to hair:
Render all splines within a curve object as hair in one click in the UI, also easily scriptable.
Radii variation:
If you modify the radius at each point of your spline or b-spline, this will be affect the hair (see image above).
Per-spline material:
This modification allows to have per-spline materials. You need to add the materials to the curve object and then set the material_index of each spline.
Value (beta):
In the Hair Info node of Cycles, the Value slot is now added. It allows you to pass a value per spline point to the weight_softbody property and use it for modulating materials. You may pass any float between 0.5 and 100 to weight_softbody attribute of spline or b-spline points. Why weight_softbody? Because this is the only shared property between b-splines and splines in Blender. A real "value" property could be added later. 

## How to use it?
You need to compile Blender with these sources. You need to have git installed on your system or skip step 1 and download sources from github.
In your console run the following commands (valid for Linux/MacOS/Windows):
1. `git clone git://github.com/nantille/blender_28_mod`
2. `cd blender_28_mod`
3. `make`

If everything compiles fine, you will have a new Blender executable in relative path `../build_[platform]/bin/`

If you would like to compile Blender with the latest sources published by Blender Institute,
you can try rebasing with the following command:
`git pull --rebase http://git.blender.org/blender.git master`

## Version
This is a modification for Blender 2.8+.

## Usage
1. Open this custom version of Blender 2.8.
2. Select Cycles as render engine.
3. Select or add a curve object on the scene.
4. Under `Object Data` panel at the bottom, enable `Render as Hair`.
5. Make sure that in panel `Scene`, under `Geometry`, the option `Use Hair` is enabled.
6. Tweak options under `Use Hair`, set `Shape` to `Thick` and `Primitive` to `Curve Segments`.

## You can't have it all
Please note that when a spline is rendered as a hair primitive, it has *no mesh and thus all mesh related features of splines/curves will be ignored!*.
Also currently the interpolation of hair primitives doesn't match all possible interpolations that splines and b-splines offer. This could be improved too.

Typical features that are not supported (mesh related):
- no support of shape parameters like fill, twisting and resolution
- no uv mapping, though you can get U coordinate with HairInfo>Intercept multiplied with the new HairInfo>SplineLength.
- no taper/bevel object
- no cyclic property (though if people need it, it could be added)
- no nurbs interpolation and resolution (currently only hair interpolation for curves)

## Testing
This example creates 1000 (n) splines within one curve object and sets random radii and weights (for HairInfo>Value).
`
import numpy as np
import bpy
import random

def create_spline(curve_data, s_type='NURBS', len_nodes=100, bud_position=None):
    s_type = 'NURBS'
    spline = curve_data.splines.new(type=s_type)
    
    # Regular spline points need xyz + weight
    got_points = 1
    co_dimension = 4
    pts = spline.points
    if s_type == 'BEZIER':
        got_points = 2
        # Bezier control points accept only xyz
        co_dimension = 3
        # Left and right handles are not handled here
        pts = spline.bezier_points
    
    # Spline already has got point(s) when created
    spline.points.add(len_nodes - got_points)
    
    if bud_position is None:
        bud_position = np.random.rand(1, co_dimension) * 100
    
    radii = np.random.rand(len_nodes)
    radii *= radii**2
    rnd_walk = np.arange(len_nodes-1)
    rnd_walk += 1
    rnd_walk *= 3.1415926535 / (len_nodes/2)
    nodes = np.random.rand(len_nodes, co_dimension)
    nodes[:, 0] += np.sin(rnd_walk) * 10
    nodes[:, 1] += np.cos(rnd_walk) * 10
    nodes[:, 2] += np.sin(rnd_walk*2) * np.cos(rnd_walk*2) * 10
    nodes[:, :] *= 10
    nodes += bud_position
    # weight_softbody modifies values below 0.5, max is 100
    values = np.random.rand(len_nodes) + 0.5
    
    pts.foreach_set('radius', radii.ravel())
    pts.foreach_set('co', nodes.ravel())
    pts.foreach_set('weight_softbody', values.ravel())
    
    spline.use_endpoint_u = True
    return spline


def create_random_splines_within_curve(n_splines=100, n_nodes_per_spline=100, name='Curve > hair'):
    curve_data = bpy.data.curves.new(name=name, type='CURVE')
    curve_data.dimensions = '3D'
    curve = bpy.data.objects.new(name=name, object_data=curve_data)

    bezier_or_not = np.random.rand(n_splines)
    for spline_id in range(n_splines):
        len_nodes = n_nodes_per_spline + random.random() * 10
        s_type = 'NURBS' if bezier_or_not[spline_id] < 0.5 else 'BEZIER'
        # curve_data equivalent to accessing curve.data
        spline = create_spline(curve_data, s_type, len_nodes)
    
    # Or use bpy.context.scene if you prefer
    bpy.data.scenes['Scene'].objects.link(curve)
    return curve
    
curve = create_random_splines_within_curve(10, 100)
# Tell Cycles to render all splines within this curve as hair 
curve.cycles_curves.render_as_hair = True
`

## Issues
None found at the moment. Don't hesitate to report any issue you see.