
# PNP_Backend

## Introduction

Ce projet contient un solveur pour le problème ["Perspective-n-Points"](https://en.wikipedia.org/wiki/Perspective-n-Point) (PnP). Il s'agit, étant donnée une photo, d'estimer les caractéristiques et la position de l'appareil de prise de vue. 

Cette estimation se fait à partir de `n` points dont on connaît à la fois les coordonnées réelles (en 3 dimensions) et les coordonnées dans la photo (en 2 dimensions). L'estimation repose sur une modélisation du comportement des appareils photo basée sur la projection perspective. D'où le nom de "Perspective-n-Points".

*Cette page indique comment installer, démarrer, et utiliser le solveur.*

*Pour en savoir plus sur les bases théoriques de ce projet (modélisation et résolution du problème), consultez [notre wiki](https://github.com/Iconem/PNP_Backend/wiki/Th%C3%A9orie-du-probl%C3%A8me-PnP).*

## Implémentation

Le **solveur** en lui-même est implémenté en *C++*, avec l'aide de la librarie [openCV](https://opencv.org/).

Un **serveur web** vient compléter l'implémentation : il permet de rendre le solveur accessible depuis une machine distante, et facilite la communication avec le solveur grâce à l'utilisation du format *JSON*. Ce serveur est codé en *nodeJS*.

Enfin, l'application (solveur + serveur) est encapsulée dans un **conteneur** [docker](https://www.docker.com/). Cela permet de l'installer sur n'importe quelle type de machine (windows, mac, linux), avec pour seule dépendance docker lui-même. 


## Installation

- Assurez-vous d'avoir **docker** installé sur votre machine. Sinon, consultez la [page de téléchargement](https://docs.docker.com/get-docker/) et installez la version adaptée à votre machine.
- Téléchargez ce repo.
- Optionnel : téléchargez l'application [Postman](https://www.postman.com/downloads/) pour effectuerp lus facilement des requêtes vers le serveur.


## Usage

### Démarrage du serveur

Pour démarrer le serveur, lancez la commande : `docker-compose up`.

Le premier démarrage peut être long (quelques minutes), car les dépendances sont téléchargées et compilées dans le conteneur. Cette étape ne sera pas exécutée aux démarrages suivants, les rendant bien plus rapides (de l'ordre de quelques secondes). Le serveur est prêt à recevoir des requêtes dès qu'il affiche le message *"PnP solver running on port 3000. Waiting for requests..."*.

Pour vérifier votre configuration, entrez l'adresse *"localhost:3000"* dans un navigateur. Il devrait afficher la phrase *"Hello from PnP solver"*.


### Effectuer un calcul

Pour lancer un calcul, on envoie une requête **POST** sur la route **localhost:3000/solve**. Le corps de la requête doit être au format JSON, et respecter le format suivant :

```js
{
	// tableau contenant les correspondances 2D-3D.
	// Doit contenir au minimum 6 correspondances
	bindings: [ 
		{
			// coordonnées d'un point réel
			point3D: { x: Number, y: Number, z: Number},
			// coordonnées du point correspondant sur la photo
			point2D: { x: Number, y: Number }
		}
		//...
	],
	// dimensions de la photo en pixels
	imageDimensions: {
		width: Number, // largeur de l'image en pixels
		height: Number // hauteur de l'image en pixels
	}
}
```

Attention, **les coordonnées 2D doivent être comprises entre -1 et 1** (le coin inférieur gauche de la photo ayant pour coordonnées [-1, -1]).


### Interpréter le résultat

La réponse du serveur contient une estimation des paramètres de l'appareil ayant pris la photo. Elle contient également une estimation de l'erreur globale, et de l'erreur par point :
```js
{
	camera: {
		// la matrice de la caméra, 
		// indiquant le positionnement de l'appareil en 3d
		matrix: [Number(16)], 
		// angle de vue vertical (en degrés)
		vFOV: Number,
		// déviation horizontale du centre optique de l'appareil
		cx: Number,
		// déviation verticale du centre optique de l'appareil
		cy: Number,
		// largeur de la photo en pixels
		width: Number,
		// hauteur de la photo en pixels
		height: Number,
	},
	errorEstimation: {
		// tableau contenant les erreurs par point
		bindingsError: [Number(n)], 
		// estimation de l'erreur globale
		globalError: Number
	}
}
```
Attention : le champ **matrix** correspond aux éléments de la matrice selon le format adopté par *THREEjs* (cf https://threejs.org/docs/#api/en/math/Matrix4)
 
L'appareil peut ainsi être simulé en *THREEjs* de la façon suivante :
```js
function simulateCamera(camera) { 
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
}
```
## Développement

### Solveur
Le code du solveur se trouve dans le fichier *src/solver/camCalibNode.cpp*. En cas de mise à jour, il doit être recompilé. Pour cela, stoppez le conteneur docker (Ctrl-C) et relancez-en un nouveau avec l'option `--build`, pour forcer la compilation :
`docker-compose up --build`

### Serveur
Le code du serveur se trouve dans le dossier *src/server*. Il n'est pas nécessaire de relancer le conteneur docker lorsque vous mettez à jour ce code : la mise à jour est faite automatiquement.
