
# PNP_Backend

## Introduction

Ce projet contient un solveur pour le problème ["Perspective-n-Points"](https://en.wikipedia.org/wiki/Perspective-n-Point) (PnP). Il s'agit, étant donnée une photo, d'estimer les caractéristiques et la position de l'appareil de prise de vue. 

Cette estimation se fait à partir de `n` points dont on connaît à la fois les coordonnées réelles (en 3 dimensions) et les coordonnées dans la photo (en 2 dimensions). L'estimation repose sur une modélisation du comportement des appareils photo basée sur la projection perspective. D'où le nom de "Perspective-n-Points".


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

## Bases théoriques

### Notations

Dans ce qui suit, on utilise les notations suivantes:
- $R_{w\rightarrow c} \in M_{3,3}$ représente la rotation de la caméra dans le repère-monde (w: world).
- $t_{w\rightarrow c} \in \mathbb{R}\_3$ représente la position de la caméra dans le repère monde.
$f$ représente la distance focale de la caméra.
$h$ and $w$ représentent les dimensions de la photo en pixels.

- $P^{3D}_w = \begin{pmatrix}
  x^{3D}_w\\ 
  \\
  y^{3D}_w\\ 
  \\
  z^{3D}_w
\end{pmatrix}$ désigne un point de l'espace, dans le repère monde.
- $P^{3D}_c = \begin{pmatrix}
  x^{3D}_c\\ 
  \\
  y^{3D}_c\\ 
  \\
  z^{3D}_c
\end{pmatrix}$ désigne le même point, dans le repère de la caméra.
- $P^{proj}_c = \begin{pmatrix}
  x^{proj}_c\\ 
  \\
  y^{proj}_c\\ 
  \\
  z^{proj}_c
\end{pmatrix}$ désigne la projection de $P^{3D}$ dans le plan de la caméra.

![pinhole_camera.png](/platform-docs/general/pinhole_camera.png)

On pose également que l'image correspond au domaine $y_c \in [-\frac{1}{2}, \frac{1}{2}], x_c \in [-\frac{1}{2}, \frac{1}{2}]$ du plan de la caméra, $z_c = -f$.

- $P^{2D} = \begin{pmatrix}
  x^{2D}\\ 
  y^{2D}
\end{pmatrix}$ désigne les coordonnées du points correspondant sur l'image, en pixels.

![camera_plane.png](/platform-docs/general/camera_plane.png)

### Projection perspective

Dans cette section, on suppose qu'il n'y a pas de distorsion. On cherche à exprimer les coordonnées $P^{2D}$ d'un point sur l'image en fonction de ses coordonnées dans l'espace, $P^{3D}_w$. 

On commence par exprimer les coordonnées du point dans le repère de la caméra, $P^{3D}_c$, en fonction de $P^{3D}_w$. 

$$
 P^{3D}_c = R^{-1}_{w\rightarrow c} \times (P^{3D}_w - t_{w\rightarrow c})
$$

Maintenant que l'on connaît les coordonnées du point 3D dans le repère caméra, on exprime le projeté $P^{proj}_c$ en fonction de $P^{3D}_c$. 

Par définition du plan de la caméra :
$$z^{proj}_c = -f$$ 

Pour $x^{proj}_c$ et $y^{proj}_c$, on applique le théorème de Thalès :

$$
\begin{matrix}
x^{proj}_c = -f\frac{x^{3D}_c}{z^{3D}_c} \\
\\
y^{proj}_c = -f\frac{y^{3D}_c}{z^{3D}_c} 
\end{matrix}
$$

ce qui, normalisé dans le plan image $y_c \in [-\frac{1}{2}, \frac{1}{2}], x_c \in [-\frac{1}{2}, \frac{1}{2}]$, donne :
$$
P^{proj}_c = \begin{pmatrix}
-f \times \frac{x^{3D}_c}{z^{3D}_c} \\
\\
-\frac{fw}{h} \times \frac{y^{3D}_c}{z^{3D}_c}
\end{pmatrix}
$$
Pour finir, on en déduit les coordonnées-image, $P^{2D}$ :

$$
P^{2D} = \begin{pmatrix}
-x^{proj}_c \times w = f \times w \times \frac{x^{3D}_c}{z^{3D}_c} \\
\\
 -y^{proj}_c \times h = \frac{fw}{h} \times h \times \frac{y^{3D}_c}{z^{3D}_c}
\end{pmatrix}
$$

### Prise en compte de la distorsion

Nous avons adopté un modèle simplifié dans lequel la distorsion est dûe uniquement à un décalage du centre optique de la caméra. Autrement dit, le centre optique de la caméra a pour coordonnées $(c_x, c_y) \neq (0,0)$, ce qui a pour conséquence de décaler les points de la photo :
$$
P^{proj}_c = \begin{pmatrix}
-f \times \frac{x^{3D}_c}{z^{3D}_c} - c_x\\
\\
-\frac{fw}{h} \times \frac{y^{3D}_c}{z^{3D}_c} -c_y
\end{pmatrix}
$$
puis :
$$
P^{2D} = \begin{pmatrix}
fw \times \frac{x^{3D}_c}{z^{3D}_c} + c_xw\\
\\
\frac{fw}{h} \times h \times \frac{y^{3D}_c}{z^{3D}_c} + c_yh
\end{pmatrix}
$$

### Matrice intrinsèque
En généralisant le raisonnement précédent, on peut résumer le lien entre un point $P^{3D}_w$ de l'espace et le point projeté $P^{proj}_c$ :

$$
 P^{proj}_c =  K \times [I_3 | 0] \times [R_{w\rightarrow c} | t_{w\rightarrow c}] \times P^{3D}_w
$$
où $K = \begin{pmatrix}
  f & 0 & c_x\\ 
  \\
  0 & \frac{fw}{h} & c_y \\ 
  \\
  0 & 0 & 1
\end{pmatrix}$ est appelée *matrice intrinsèque* de la caméra.


### Problème PnP
Le problème PnP consiste à estimer les paramètres intrinsèques (focale $f$ et décentrage $(c_x, c_y)$) et extrinsèques (position $t$ et rotation $R$) de la caméra.
