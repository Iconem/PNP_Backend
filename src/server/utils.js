const THREE = require('three');

function bindingsToSolverArguments(bindings) {
	// shift 3D point positions to obtain better precision,
  	// and divide 2D point positions by 2
  	const firstPoint3D = bindings[0].point3D;
  	const shift = {
  	  x: firstPoint3D.x,
  	  y: firstPoint3D.y,
  	  z: firstPoint3D.z
  	};
  	const shiftedBindings = bindings.map(item => {
  	  return {
  	    point3D: {
  	      x: item.point3D.x - shift.x + 1,
  	      y: item.point3D.y - shift.y + 1,
  	      z: item.point3D.z - shift.z + 1
  	    },
  	    point2D: {
  	      x: item.point2D.x / 2,
  	      y: item.point2D.y / 2
  	    }
  	  };
  	});

  	// build arguments for C++ solver
  	let args = '-- ';
  	args += shiftedBindings.length;
  	shiftedBindings.map(item => {
  	  args += ' ' + item.point3D.x;
  	  args += ' ' + item.point3D.y;
  	  args += ' ' + item.point3D.z;
  	  args += ' ' + item.point2D.x;
  	  args += ' ' + item.point2D.y;
  	});
  	return { shift: shift, args: args };
}

function solverOutputToCamera(solverOutput, shift, imageDimensions) {
	const params = solverOutput.split(/\s+/);

	// create fake THREEjs object
	// and assign its position and rotation
	// => get the camera matrix from the object
	const obj = new THREE.Object3D();
	const position = new THREE.Vector3(
	  parseFloat(params[0]) + shift.x - 1,
	  parseFloat(params[1]) + shift.y - 1,
	  parseFloat(params[2]) + shift.z - 1
	);
	obj.position.copy(position);
	obj.rotation.set(
	  -parseFloat(params[3]),
	  parseFloat(params[4]) - Math.PI,
	  parseFloat(params[5]),
	  'ZYX'
	);
	obj.updateMatrix();
	const cameraMatrix = obj.matrix.elements;

	// return camera
	const camera = {
		matrix: cameraMatrix,
		cx: parseFloat(params[8]),
		cy: parseFloat(params[9]),
		vFOV: parseFloat(params[10]),
		width: imageDimensions.width,
		height: imageDimensions.height
	}
	return camera;
}

function computeErrorEstimation(camera, bindings) {
	// build a fake 3D camera from camera parameters
	const { matrix, cx, cy, vFOV, width, height } = camera;
	const fakeCamera3D = new THREE.PerspectiveCamera(vFOV, width / height, 1, 1000);
	const m = new THREE.Matrix4().set(...matrix).transpose();
	const position = new THREE.Vector3();
	const quaternion = new THREE.Quaternion();
	const scale = new THREE.Vector3();
	m.decompose(position, quaternion, scale);
	fakeCamera3D.position.copy(position);
	fakeCamera3D.quaternion.copy(quaternion);
	fakeCamera3D.setViewOffset(width, height, width * cx, height * cy, width, height);

	fakeCamera3D.updateMatrix();
	fakeCamera3D.updateMatrixWorld();
	fakeCamera3D.updateProjectionMatrix();
	
	// for each 2D-3D point correspondence,
	// reproject the 3D point in the fake camera world
	// then compare the projection to the original 2D point
	// (dist in pixels)
	let globalError = 0;
	const bindingErrors = [];
	bindings.map(binding => {
	  const p3D = new THREE.Vector3().fromArray([
	    binding.point3D.x,
	    binding.point3D.y,
	    binding.point3D.z
	  ]);
	  const projectedP3D = p3D.clone().project(fakeCamera3D);
	  const pImg3D = {
	    x: ((projectedP3D.x + 1) * width) / 2, // x between 0 and width
	    y: ((projectedP3D.y + 1) * height) / 2 // y between 0 and height
	  };
	  const pImg2D = {
	    x: ((binding.point2D.x + 1) * width) / 2, // x between 0 and width
	    y: ((binding.point2D.y + 1) * height) / 2 // y between 0 and height
	  };
	  const error = {
	    x: Math.abs(pImg3D.x - pImg2D.x) / width, // horizontal error in px / width in px
	    y: Math.abs(pImg3D.y - pImg2D.y) / height // vertical error in px / height in px
	  };
	  bindingErrors.push(error);
	  globalError += (error.x + error.y) / 2;
	});

	// divide errorEstimatoin by nb of bindings
	globalError = globalError / bindings.length;

	return {
		bindingErrors,
		globalError
	};
}

module.exports = {
	bindingsToSolverArguments,
	solverOutputToCamera,
	computeErrorEstimation
}