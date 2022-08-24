const util = require('util');
const execFile = util.promisify(require('child_process').execFile);
const path = require('path');
const utils = require('./utils');

function verifyBodyContent(req, res, next) {
  const { bindings, imageDimensions } = req.body;
  const errors = [];

  // verify bindings
  if (!bindings) {
    errors.push('Missing parameter: bindings');
  } else if (!Array.isArray(bindings)) {
    errors.push('Bad parameter bindings: you must provide an array of at least 6 objects of the form { point2D, point3D }');
  } else if (bindings.length < 6) {
    errors.push('Bad parameter bindings: you must provide at least 6 bindings');
  } else {
    bindings.map((binding, index) => {
      const { point3D, point2D } = binding;
      if (!point3D || !point2D) {
        errors.push(`Bad parameter bindings[${index}]: binding object must be of the form { point2D, point3D }`);
      } else if (typeof point3D.x === 'undefined' || typeof point3D.y === 'undefined' || typeof point3D.z === 'undefined') {
        errors.push(`Bad parameter bindings[${index}].point3D: binding.point3D object must be of the form { x, y, z }`);
      } else if (typeof point2D.x === 'undefined' || typeof point2D.y === 'undefined') {
        errors.push(`Bad parameter bindings[${index}].point2D: binding.point2D object must be of the form { x, y }`);
      } else {
        if ( typeof point3D.x !== 'number' || typeof point3D.y !== 'number' || typeof point3D.z !== 'number') {
          errors.push(`Bad parameter "bindings[${index}].point3D: 3D coordinates must be numbers`);
        }
        if ( typeof point2D.x !== 'number' || typeof point2D.y !== 'number') {
          errors.push(`Bad parameter bindings[${index}].point2D: 2D coordinates must be numbers`);
        }
        if (point2D.x < -1 || point2D.x > 1 || point2D.y < -1 || point2D.y > 1) {
          errors.push(`Bad parameter bindings[${index}].point2D: 2D coordinates must be between -1 and 1`);
        }
      }
    });
  }

  // verify imageDimensions
  if (!imageDimensions) {
    errors.push('Missing parameter: imageDimensions');
  } else {
    const { width, height } = imageDimensions;
    if (typeof width === 'undefined' || typeof height === 'undefined') {
      errors.push('Bad parameter imageDimensions : imageDimensions must be of the form { width, height }');
    } else if ( typeof width !== 'number' || typeof height !== 'number') {
      errors.push('Bad parameter imageDimensions: width and height must be numbers');
    }
  }

  if (errors.length > 0) {
    return res.status(422).json(errors);
  } else {
    return next();
  }
}

async function solve(req, res, next) {
  try {
    const { bindings, imageDimensions } = req.body;

    // get solver arguments
    const { shift, args } = utils.bindingsToSolverArguments(bindings);

    // launch solver
    const solverPath = path.join(__dirname, '../solver/build/camCalibNode');
    const { stdout } = await execFile(solverPath, args.split(' '));

    // verify solver's output
    if (stdout === '0') {
      throw new Error('An error occurred');
    }

    // build camera from solver's output
    const camera = utils.solverOutputToCamera(stdout, shift, imageDimensions);

    // compute error estimation
    const errorEstimation = utils.computeErrorEstimation(camera, bindings);

    // send result
    res.status(200).json({
      camera,
      errorEstimation
    });
  } catch (error) {
    return next(error);
  }
}

module.exports = {
  verifyBodyContent,
  solve
}