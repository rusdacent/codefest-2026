- [NeuVector](#neuvector)
  - [Требования к стенду](#требования-к-стенду)
    - [Установка k3s](#установка-k3s)
    - [Установка Helm](#установка-helm)
    - [Установка NeuVector](#установка-neuvector)
      - [Проверка enforcer'а](#проверка-enforcerа)
    - [Доступ к Web UI](#доступ-к-web-ui)
    - [Демо-окружение](#демо-окружение)
  - [Демо 1. Сетевые правила и автообучение](#демо-1-сетевые-правила-и-автообучение)
    - [1.1. Автообучение](#11-автообучение)
    - [1.2. Генерация легитимного трафика](#12-генерация-легитимного-трафика)
  - [Демо 2. Блокировка сетевого соединения](#демо-2-блокировка-сетевого-соединения)
    - [2.1. Запуск атакующего пода](#21-запуск-атакующего-пода)
    - [2.2. Режим Discover](#22-режим-discover)
    - [2.3. Режим Protect](#23-режим-protect)
    - [2.4. Удаление авто-правила](#24-удаление-авто-правила)
    - [2.5. Проверка Protect](#25-проверка-protect)
  - [Демо 3. Блокировка процесса](#демо-3-блокировка-процесса)
    - [3.1. Добавление Deny-правила](#31-добавление-deny-правила)
    - [3.2. Проверка блокировки процесса](#32-проверка-блокировки-процесса)
  - [Демо 4. Сканирование уязвимостей](#демо-4-сканирование-уязвимостей)
  - [Демо 5. Admission Control](#демо-5-admission-control)
    - [5.1. Включить Admission Control](#51-включить-admission-control)
    - [5.2. Создание правила](#52-создание-правила)
    - [5.3. Запуск образа не из quay.io](#53-запуск-образа-не-из-quayio)
    - [5.4. Запуск образа из quay.io](#54-запуск-образа-из-quayio)
  - [Демо 6. Compliance](#демо-6-compliance)
  - [Очистка кластера](#очистка-кластера)

# NeuVector

## Требования к стенду

### Установка k3s

```bash
curl -sfL https://get.k3s.io | sh -
```

Настройка kubeconfig

```bash
mkdir -p $HOME/.kube
sudo cp /etc/rancher/k3s/k3s.yaml $HOME/.kube/config
sudo chown $(id -u):$(id -g) $HOME/.kube/config
```

Проверяем состояние кластера

```bash
kubectl get nodes
kubectl get pods -A
```

### Установка Helm

```bash
curl -fsSL https://raw.githubusercontent.com/helm/helm/main/scripts/get-helm-4 | bash
```

### Установка NeuVector

```bash
helm repo add neuvector https://neuvector.github.io/neuvector-helm
helm repo update

helm install neuvector neuvector/core -n neuvector --create-namespace \
    --set controller.pvc.enabled=false \
    --set manager.svc.type=ClusterIP \
    --set runtimePath=/run/k3s/containerd/containerd.sock \
    --set bootstrapPassword="123QWEasd"
```

Дождаться готовности всех компонентов

```bash
kubectl rollout status -n neuvector deploy/neuvector-controller-pod
kubectl rollout status -n neuvector deploy/neuvector-manager-pod
kubectl rollout status -n neuvector ds/neuvector-enforcer-pod

kubectl get pods -n neuvector
```

#### Проверка enforcer'а 

`enforcer` должен видеть все контейнеры

Получаем имя пода

```bash
ENF=$(kubectl get pod -n neuvector -l app=neuvector-enforcer-pod -o jsonpath='{.items[0].metadata.name}')
```

Проверяем состояние

```bash
kubectl get pod -n neuvector $ENF
```

```bash
kubectl logs -n neuvector $ENF | grep -iE 'containerd connected|runtime=containerd'
```

Вывод должен быть `containerd connected, runtime=containerd`

```bash
kubectl logs -n neuvector $ENF | grep -iE 'DP Connected'
```

Проверяем, что происходит регистрация контейнеров

```bash
kubectl logs -n neuvector $ENF | grep -iE 'taskAddContainer'
```

Проверяем, что нет ошибок

```bash
kubectl logs -n neuvector $ENF | grep -iE 'proc monitor.*fail|Receive mcast fails'
```

### Доступ к Web UI

Пароль `123QWEasd` задаван при деплое

Доступ к UI, либо через проброс на localhost

```bash
kubectl port-forward -n neuvector svc/neuvector-service-webui 8443:8443
```

Либо через clusterIP

```bash
CLUSTER_IP=$(kubectl get --namespace neuvector -o jsonpath="{.spec.clusterIP}" services neuvector-service-webui); TARGET_PORT=$(kubectl get --namespace neuvector -o jsonpath="{.spec.ports[0].targetPort}" services neuvector-service-webui); echo https://$CLUSTER_IP:$TARGET_PORT
```

### Демо-окружение

```bash
kubectl create namespace demo

kubectl apply -f redis.yaml -f nodejs.yaml -f nginx.yaml

kubectl get pods -n demo
kubectl get svc  -n demo
```

В UI -> **Assets** -> **Containers** - должны появиться записи `redis-pod`,
`node-pod`, `nginx-pod`.

## Демо 1. Сетевые правила и автообучение

### 1.1. Автообучение

Просмотр правил, которые сформированы на основе автообучения

UI -> **Policy** -> **Network Rules**.

Видно легитимные связи между сервисами `node-pod` `redis`, `nginx-pod` и т.д.

### 1.2. Генерация легитимного трафика

Цикл "дёргает" сервис, который считает количество посещений

```bash
for i in {1..5}; do
  curl -s http://localhost:31337 | head -1
  sleep 1
done
```

На сетевой карте видны связи между сервисами



## Демо 2. Блокировка сетевого соединения

Настроить правила таким образом, чтобы атакующий под не мог достучаться до Redis

### 2.1. Запуск атакующего пода

```bash
kubectl create deployment attacker -n demo --image=redis:alpine -- sleep 3600
```

Проверяем, что он создался

```
kubectl wait --for=condition=available deploy/attacker -n demo --timeout=60s
```

Получаем имя пода

```
ATTACKER=$(kubectl get pod -n demo -l app=attacker -o jsonpath='{.items[0].metadata.name}') ; echo ATTACKER=$ATTACKER
```

### 2.2. Режим Discover

```bash
kubectl exec -n demo $ATTACKER -- redis-cli -h redis.demo.svc.cluster.local -t 3 ping
```

Должны получить `PONG`, так как всё разрешено (включен режим Discover)

### 2.3. Режим Protect

UI -> **Policy** -> **Groups**

Для блокировки соединения `attacker` -> `redis` в Protect должны быть оба сервиса:

1. `nv.attacker.demo` -> **Switch Mode** -> **Network Policy Mode = Protect**

2. `nv.redis-pod.demo` -> **Switch Mode** -> **Network Policy Mode = Protect**

### 2.4. Удаление авто-правила

UI -> **Policy** -> **Network Rules** 

Ищем строки с `redis` и `attacker`

### 2.5. Проверка Protect

```bash
kubectl exec -n demo $ATTACKER -- redis-cli -h redis.demo.svc.cluster.local -t 3 ping
```

Ожидаем таймаут и отказ в соединении, а так же нотификацию

UI -> **Notifications** -> **Security Events**

Пример события `Network violation, src: demo/attacker, dst: demo/redis, action: Deny`

На сетевой карте должно появиться красное ребро `attacker -> redis`

## Демо 3. Блокировка процесса

Запретим запуск `curl` внутри пода `node-pod` явным правилом

### 3.1. Добавление Deny-правила

UI -> **Policy -> Groups** 

Выбераем `nv.node-pod.demo` -> **Process Profile Rules** -> **Add Rule**:

- **Action:** Deny
- **Process Path:** `curl`

Переключаем **Switch Mode** для `node` в **Process Profile Mode = Protect**.

### 3.2. Проверка блокировки процесса

```bash
NODEPOD=$(kubectl get pod -n demo -l app=node-pod -o jsonpath='{.items[0].metadata.name}') ; echo NODEPOD=$NODEPOD

kubectl exec -n demo $NODEPOD -- curl -v google.com
```

Ожидаем kill процесс с кодом 137 (SIGKILL), а так же нотификацию

UI -> **Notifications** -> **Security Events**

Присер события `Process violation, Container: node-pod.demo, Process: /usr/bin/curl, Action: Deny`.

## Демо 4. Сканирование уязвимостей

UI -> **Security Risks** -> **Vulnerabilities**.

На сетевой карте должны появиьтся красные метки, которые отмечают уязвимости

## Демо 5. Admission Control

Разрешить образы только из доверенного реджистри (`quay.io`)

### 5.1. Включить Admission Control

UI -> **Policy** -> **Admission Control**.

В верхней части страницы:

1. Переключатель **Enable admission control** -> **ON**. При включении NeuVector регистрирует в кластере `ValidatingWebhookConfiguration` - с этого момента каждый запрос на создание пода проходит через NeuVector

2. Режимы работы:
   - **Monitor** - логирует нарушения в Security Events
   - **Protect** - блокируеи создание подов

Выбираем **Protect**

### 5.2. Создание правила

UI -> **Policy** -> **Admission Control**, список правил -> кнопка **Add**:

1. Тип правила: **Deny**
2. **Add Criteria** -> критерий **image registry** -> оператор
   **is not any of** -> значение: `https://quay.io`
3. Правило должно быть в состоянии **Enabled**

### 5.3. Запуск образа не из quay.io

```bash
kubectl run test-dockerhub --image nginx:latest
```

Пример вывода

```
Error from server: admission webhook "neuvector-validating-admission-webhook..."
  denied the request: Creation of Kubernetes Pod is denied.
```

### 5.4. Запуск образа из quay.io

```bash
kubectl run test-quay --image quay.io/prometheus/busybox -- sleep 3600
kubectl get pod test-quay -w
```

Под `test-quay` должен перейти в состояние`Running`

Удаление тестовых подов

```bash
kubectl delete pod test-quay test-dockerhub --ignore-not-found
```

UI -> **Notifications** -> **Risk Report**

+ **Network Activity**

## Демо 6. Compliance

UI -> **Notifications** -> **Security Risks** -> **Compliance**.

Как пример можно посмотреть `K.1.1.20`


## Очистка кластера

Отдельные демо и поды

```bash
kubectl delete namespace demo --ignore-not-found; kubectl delete pod test-quay test-dockerhub --ignore-not-found
```

Сам NeuVector

```bash
helm uninstall neuvector -n neuvector; kubectl delete namespace neuvector --ignore-not-found

kubectl get validatingwebhookconfigurations,mutatingwebhookconfigurations -o name | grep -i neuvector | xargs -r kubectl delete; kubectl get clusterrole,clusterrolebinding -o name | grep -i neuvector | xargs -r kubectl delete; kubectl get crd -o name | grep -i neuvector | xargs -r kubectl delete
```
