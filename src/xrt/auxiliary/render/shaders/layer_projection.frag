// Copyright 2020-2023, Collabora Ltd.
//
// Author: Jakob Bornecrantz <jakob@collabora.com>
// Author: Lubosz Sarnecki <lubosz.sarnecki@collabora.com>
//
// SPDX-License-Identifier: BSL-1.0

#version 460

layout (binding = 0, std140) uniform Config
{
	vec4 post_transform;
	vec4 to_tangent;
	mat4 mvp;
	vec4 color_scale;
	vec4 color_bias;
	vec4 inset;      // x: quad-view inset soft-edge width in inset UV (0 = off), y: 1 = round fade, z: 1 = sRGB decode
} ubo;

layout (binding = 1) uniform sampler2D image;

layout (location = 0)  in vec2 uv;
layout (location = 0) out vec4 out_color;


void main ()
{
	out_color = texture(image, uv);
	if (ubo.inset.z > 0.5) { // Varjo Aero: doubly sRGB-encoded app image, decode once more
		out_color.rgb = mix(out_color.rgb / 12.92, pow((out_color.rgb + 0.055) / 1.055, vec3(2.4)),
		                    step(vec3(0.04045), out_color.rgb));
	}
	out_color = clamp(out_color * ubo.color_scale + ubo.color_bias, 0.0, 1.0);

	if (ubo.inset.x > 0.0) {
		// Position inside the inset image (undo the sub-image/flip transform), distance to its nearest border.
		vec2 t = (uv - ubo.post_transform.xy) / ubo.post_transform.zw;
		float d = min(min(t.x, 1.0 - t.x), min(t.y, 1.0 - t.y));
		if (ubo.inset.y > 0.5) {
			// Round: distance to the circle inscribed in the inset (Varjo describes its focus area as a cone).
			d = 0.5 - length(t - vec2(0.5));
		}
		out_color *= smoothstep(0.0, ubo.inset.x, d); // premultiplied: rgb and alpha together
	}
}
