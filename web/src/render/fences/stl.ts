import * as THREE from 'three';
import { STLLoader } from 'three/examples/jsm/loaders/STLLoader.js';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js';
import type { FenceRenderer } from './registry';

const renderer: FenceRenderer = {
  async render(code, el, ctx) {
    const geometry = new STLLoader().parse(code);
    if (!geometry.getAttribute('position')?.count) throw new Error('No triangles found in STL.');
    geometry.computeVertexNormals();
    geometry.center();
    geometry.computeBoundingSphere();
    const radius = geometry.boundingSphere?.radius || 1;

    const width = Math.min(el.parentElement?.clientWidth || 640, 720);
    const height = Math.round(width * 0.6);
    let gl: THREE.WebGLRenderer;
    try {
      gl = new THREE.WebGLRenderer({ antialias: true, alpha: true, preserveDrawingBuffer: true });
    } catch {
      throw new Error('3D preview needs WebGL, which is unavailable.');
    }
    gl.setPixelRatio(window.devicePixelRatio);
    gl.setSize(width, height);
    gl.domElement.className = 'om-stl';

    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(40, width / height, radius / 100, radius * 100);
    camera.position.set(radius * 1.6, radius * 1.2, radius * 2.2);
    scene.add(new THREE.HemisphereLight(0xffffff, ctx.theme === 'dark' ? 0x222222 : 0x888888, 2.2));
    const key = new THREE.DirectionalLight(0xffffff, 1.6);
    key.position.set(1, 2, 3);
    scene.add(key);
    const material = new THREE.MeshStandardMaterial({
      color: ctx.theme === 'dark' ? 0x58a6ff : 0x0969da,
      metalness: 0.1,
      roughness: 0.6,
      flatShading: true,
    });
    scene.add(new THREE.Mesh(geometry, material));

    const controls = new OrbitControls(camera, gl.domElement);
    controls.enableDamping = true;
    const frame = () => {
      if (!gl.domElement.isConnected && gl.domElement.dataset.started) {
        gl.dispose();
        gl.forceContextLoss(); // browsers cap live WebGL contexts
        return; // re-rendered away
      }
      gl.domElement.dataset.started = '1';
      controls.update();
      gl.render(scene, camera);
      requestAnimationFrame(frame);
    };
    el.append(gl.domElement);
    frame();
  },
};

export default renderer;
