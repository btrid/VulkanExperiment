cd /d %~dp0

glslangvalidator -w -V Render.vert -o Render.vert.spv
glslangvalidator -w -V Render.frag -o Render.frag.spv