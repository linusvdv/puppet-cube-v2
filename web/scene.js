// Port of gui/scene.py: the 26 piece viewer scene with the matura thesis
// lighting (per vertex color + abs(dot(normal, view axis)) - 0.5, gouraud,
// light glued to the camera, over bright vertices scaled by their max
// channel) - here as a vertex shader instead of cpu relighting.
//
// The app owns the renderer, camera and controls; every scene mode (viewer,
// editor, puppet preview) is one instance of this class rendering into them.

import * as THREE from 'three';

export const CAMERA_EYE = [3.0, 2.4, 3.6];
export const OUTLINE_OFFSET = 0.004;

const VERT = `
    attribute vec3 color;
    varying vec3 vColor;
    void main() {
        // view space normal; the light is glued to the camera (view axis)
        vec3 n = normalize(normalMatrix * normal);
        float diffusion = abs(n.z) - 0.5;
        vec3 lit = color + diffusion;
        // over bright vertices scale by the max channel (preserves ratios)
        float m = max(lit.r, max(lit.g, lit.b));
        if (m > 1.0) lit /= m;
        vColor = clamp(lit, 0.0, 1.0);
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
    }
`;

const FRAG = `
    varying vec3 vColor;
    void main() {
        gl_FragColor = vec4(vColor, 1.0);
    }
`;

export class PuppetScene {
    constructor(meshData, opts = {}) {
        this.meshData = meshData;
        this.lighting = opts.lighting !== false;

        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0.12, 0.12, 0.15);

        this.litMaterial = new THREE.ShaderMaterial({
            vertexShader: VERT, fragmentShader: FRAG,
            side: THREE.DoubleSide,  // the matura meshes have mixed winding
        });
        this.flatMaterial = new THREE.MeshBasicMaterial({vertexColors: true, side: THREE.DoubleSide});
        this.outlineMaterial = new THREE.LineBasicMaterial({color: 0x000000});

        this.pieces = [];
        for (const piece of meshData.pieces) {
            const group = new THREE.Group();
            group.add(new THREE.Mesh(this.buildGeometry(piece, piece.groups.map(g => g.color)),
                this.lighting ? this.litMaterial : this.flatMaterial));
            group.add(new THREE.LineSegments(
                this.buildOutline(piece), this.outlineMaterial));
            this.pieces.push(group);
            this.scene.add(group);
        }
    }

    buildGeometry(piece, groupColors) {
        const count = piece.vertices.length;
        const positions = new Float32Array(count * 3);
        const normals = new Float32Array(count * 3);
        const colors = new Float32Array(count * 3);
        for (let i = 0; i < count; i++) {
            positions.set(piece.vertices[i], i * 3);
            normals.set(piece.normals[i], i * 3);
        }
        const faces = [];
        for (let g = 0; g < piece.groups.length; g++) {
            const color = groupColors[g];
            for (const f of piece.groups[g].faces) {
                faces.push(...f);
                for (const v of f) {
                    colors[v*3] = color[0];
                    colors[v*3+1] = color[1];
                    colors[v*3+2] = color[2];
                }
            }
        }
        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        geometry.setAttribute('normal', new THREE.BufferAttribute(normals, 3));
        geometry.setAttribute('color', new THREE.BufferAttribute(colors, 3));
        geometry.setIndex(faces);
        return geometry;
    }

    buildOutline(piece) {
        // lines: k segments as point pairs [[x,y,z],[x,y,z]]
        const positions = new Float32Array(piece.lines.length * 2 * 3);
        let i = 0;
        for (const pair of piece.lines) {
            for (const pt of pair) {
                positions.set([pt[0]*(1+OUTLINE_OFFSET), pt[1]*(1+OUTLINE_OFFSET), pt[2]*(1+OUTLINE_OFFSET)], i * 3);
                i++;
            }
        }
        const geometry = new THREE.BufferGeometry();
        geometry.setAttribute('position', new THREE.BufferAttribute(positions, 3));
        return geometry;
    }

    // {(piece idx, group idx): rgb} - recolors stickers (the editor)
    recolorGroups(colors) {
        for (const key of Object.keys(colors)) {
            const [p, g] = key.split(',').map(Number);
            const color = colors[key];
            const mesh = this.pieces[p].children[0];
            const attr = mesh.geometry.getAttribute('color');
            for (const f of this.meshData.pieces[p].groups[g].faces) {
                for (const v of f) {
                    attr.setXYZ(v, color[0], color[1], color[2]);
                }
            }
            attr.needsUpdate = true;
        }
    }

    updatePoses(cube, angle) {
        for (let i = 0; i < this.pieces.length; i++) {
            const rot = cube.pose(i, angle);
            const m = new THREE.Matrix4().set(
                rot[0][0], rot[0][1], rot[0][2], 0,
                rot[1][0], rot[1][1], rot[1][2], 0,
                rot[2][0], rot[2][1], rot[2][2], 0,
                0, 0, 0, 1);
            this.pieces[i].quaternion.setFromRotationMatrix(m);
        }
    }

    // static rotations (the editor scene is not animated). Applied as
    // DIRECT matrices: the editor's corner placements include improper
    // (det -1) axis-sign transforms - a mirrored mesh renders fine with
    // the DoubleSide + abs() lighting, but a quaternion cannot represent
    // a reflection.
    setRotations(rotations) {
        for (let i = 0; i < this.pieces.length; i++) {
            const rot = rotations[i];
            const piece = this.pieces[i];
            piece.matrixAutoUpdate = false;
            piece.matrix.set(
                rot[0][0], rot[0][1], rot[0][2], 0,
                rot[1][0], rot[1][1], rot[1][2], 0,
                rot[2][0], rot[2][1], rot[2][2], 0,
                0, 0, 0, 1);
            piece.matrixWorldNeedsUpdate = true;
        }
    }
}

// the shared renderer + camera + controls of all scene modes
import { OrbitControls as OrbitControlsImpl } from './libs/OrbitControls.js';

export class Viewport {
    constructor(container) {
        this.renderer = new THREE.WebGLRenderer({antialias: true});
        this.renderer.setSize(container.clientWidth, container.clientHeight);
        this.renderer.setPixelRatio(window.devicePixelRatio);
        container.appendChild(this.renderer.domElement);

        this.camera = new THREE.PerspectiveCamera(
            45, container.clientWidth / container.clientHeight, 0.1, 100);
        this.camera.position.set(...CAMERA_EYE);

        this.controls = new OrbitControlsImpl(this.camera, this.renderer.domElement);
        this.controls.target.set(0, 0, 0);
        this.controls.enableDamping = true;

        this.container = container;
        window.addEventListener('resize', () => this.resize());
    }

    resize() {
        const w = this.container.clientWidth, h = this.container.clientHeight;
        this.renderer.setSize(w, h);
        this.camera.aspect = w / h;
        this.camera.updateProjectionMatrix();
    }

    render(scene) {
        // the layout changes as the timeline fills in - track the container
        const w = this.container.clientWidth, h = this.container.clientHeight;
        if (w !== this._w || h !== this._h || this._w === undefined) {
            this._w = w; this._h = h;
            this.renderer.setSize(w, h);
            this.camera.aspect = w / h;
            this.camera.updateProjectionMatrix();
        }
        this.controls.update();
        this.renderer.render(scene.scene, this.camera);
    }
}
