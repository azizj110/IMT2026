# Journal des changements (état initial → état actuel)

## 1) Objectif fonctionnel du projet

Implémenter un mode Monte Carlo à **paramètres constants** pour 3 engines custom :

- `MCEuropeanEngine_2`
- `MCDiscreteArithmeticASEngine_2`
- `MCFixedLookbackEngine_2`

Comportement attendu :

- `withConstantParameters(false)` : comportement inchangé (processus original `GeneralizedBlackScholesProcess`)
- `withConstantParameters(true)` : extraction d’un triplet constant `(r, q, sigma)` à maturité (et strike pour la vol), puis simulation avec un processus constant.

---

## 2) Fichiers ajoutés / complétés

### `constantblackscholesprocess.hpp` / `constantblackscholesprocess.cpp`

### État initial
- Fichiers quasi vides.
- Classe `ConstantBlackScholesProcess` déclarée mais non implémentée.

### Changements réalisés
- Implémentation complète de `ConstantBlackScholesProcess` dérivée de `StochasticProcess1D` :
  - stockage de `x0`, `r`, `q`, `sigma`
  - surcharge de `x0`, `drift`, `diffusion`, `expectation`, `variance`, `stdDeviation`, `evolve`, `apply`
- Ajout d’un helper partagé :
  - `makeMonteCarloProcess(process, exercise, strike, constantParameters)`
  - si `false` → retourne le process original
  - si `true` → extrait `r(T)`, `q(T)`, `sigma(T, K)` et retourne `ConstantBlackScholesProcess`

### Pourquoi
- Éviter des appels répétés coûteux vers les term structures dans les boucles MC.
- Centraliser la logique d’extraction des paramètres pour éviter la duplication dans les 3 engines.

---

## 3) `mceuropeanengine.hpp`

### État initial
- Méthode `withConstantParameters(bool)` présente mais vide (no-op).
- Pas d’état interne `constantParameters_`.
- Engine construit toujours avec le process original.

### Changements réalisés
- Ajout d’un booléen membre `constantParameters_` (builder + engine).
- `withConstantParameters(bool)` stocke la valeur.
- Le booléen est propagé dans le constructeur de l’engine.
- `pathGenerator()` choisit le process via `makeMonteCarloProcess(...)`.
- Alignement des pointeurs sur `ext::shared_ptr` (compat QuantLib moderne).

### Pourquoi
- Rendre le flag fonctionnel, sans changer l’API de `main.cpp`.

---

## 4) `mc_discr_arith_av_strike.hpp` (Asian)

### État initial
- `withConstantParameters(bool)` existait mais ne faisait rien.
- Pas de stockage du flag, pas de branchement de process.

### Changements réalisés
- Ajout de `constantParameters_` dans builder et engine.
- Propagation du flag à la construction.
- `pathGenerator()` utilise `makeMonteCarloProcess(...)`.
- `pathPricer()` conserve la logique produit (payoff/discount) d’origine.

### Pourquoi
- Activer réellement le mode constant pour l’Asian, avec changement minimal hors génération de trajectoires.

---

## 5) `mclookbackengine.hpp` (Lookback)

### État initial
- `withConstantParameters(bool)` no-op.
- `pathGenerator()` utilise toujours `process_`.
- `calculate()` inline présent dans la classe initiale.

### Changements réalisés
- Ajout de `constantParameters_` (builder + engine), et propagation.
- `pathGenerator()` bascule vers `makeMonteCarloProcess(...)` quand activé.
- Ajustements de compilation (template defaults, `calculate() const override` défini dans le header si nécessaire).

### Point sensible (écart observé)
- Le **non-constant lookback** a divergé de l’engine original quand la logique de path pricer a été modifiée.
- Cause probable : modification de la définition du min/max de trajectoire (ex. `front()` vs `begin()+1`, gestion `minmax`, ou remplacement de pricer).

### Recommandation de stabilisation
- Conserver la logique de pricer d’origine pour le lookback.
- Ne changer que la source du process dans `pathGenerator()`.

---

## 6) Fichiers de projet / outillage

### `.clang-format`
- Ajout/réglage du formatage C++17 (LLVM base, indent 4, includes triés, etc.).

### `.gitignore`
- Ajout des patterns build locaux (`main`, `*.o`, etc.).

### `changes.md`
- Création d’un fichier de traçabilité des changements.

---

## 7) Résultat fonctionnel attendu

- **Old engine** ≈ **Non constant** en NPV (même modèle).
- **Constant** peut être identique (si structures quasi plates) ou légèrement différent.
- Les gains de perf ne sont pas garantis sur un run unique (bruit de mesure).

---

## 8) Problèmes rencontrés pendant l’itération (historique)

- Conflits Git laissés dans `mclookbackengine.hpp` (`<<<<<<<`, `=======`, `>>>>>>>`).
- `#endif` dupliqués / include guard cassé.
- Erreurs de template defaults sur `MakeMCFixedLookbackEngine_2`.
- Déclarations/implémentations incohérentes de `calculate()` (classe template).
- Tentative d’utiliser un symbole `LookbackFixedPathPricer` non disponible publiquement selon version QuantLib installée.

Ces points expliquent les erreurs de build observées à différents moments.