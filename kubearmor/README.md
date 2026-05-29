- [KubeArmor](#kubearmor)
  - [Требования к стенду](#требования-к-стенду)
    - [Активация BPF-LSM](#активация-bpf-lsm)
    - [Отключение swap](#отключение-swap)
    - [Установка k3s](#установка-k3s)
    - [Установка Helm](#установка-helm)
    - [Установка оператора KubeArmor](#установка-оператора-kubearmor)
    - [Подготовка образа](#подготовка-образа)
    - [Дкплой](#дкплой)
  - [Демонстрация](#демонстрация)
  - [Очистка кластера](#очистка-кластера)

# KubeArmor

## Требования к стенду

Сценарий написан для виртуальной машины Ubuntu 24.04 VM и оркестратора k3s.

### Активация BPF-LSM

Так как на Ubuntu 24.04 `bpf` не входит в дефолтный активный LSM-список, то нужно добавить опции в GRUB для загрузки

Сначала проверяем, что ядро вообще умеет BPF-LSM

```bash
grep BPF_LSM /boot/config-$(uname -r)
```

Вывод должен быть `CONFIG_BPF_LSM=y`

Добавляем `lsm=...,bpf` к `GRUB_CMDLINE_LINUX_DEFAULT`

```bash
sudo cp /etc/default/grub /etc/default/grub.bak

sudo sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT="\(.*\)"/GRUB_CMDLINE_LINUX_DEFAULT="\1 lsm=lockdown,capability,landlock,yama,apparmor,ima,evm,bpf"/' /etc/default/grub

grep GRUB_CMDLINE_LINUX_DEFAULT /etc/default/grub
```

Вывод должен быть `lsm=lockdown,capability,landlock,yama,apparmor,ima,evm,bpf`

Обновляем GRUB м перезагружаем виртуальную машину

```bash
sudo update-grub
sudo reboot
```

После ребута проверяем, что опции отработали

```bash
cat /proc/cmdline
cat /sys/kernel/security/lsm
```

### Отключение swap

Единоразово до перезагрузки

```bash
sudo swapoff -a
```

На постоянку

```bash
sudo sed -i '/\sswap\s/s/^/#/' /etc/fstab
```

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

### Установка оператора KubeArmor

```bash
helm repo add kubearmor https://kubearmor.github.io/charts
helm repo update

helm upgrade --install kubearmor-operator kubearmor/kubearmor-operator \
  -n kubearmor --create-namespace

kubectl -n kubearmor rollout status deploy/kubearmor-operator

kubectl apply -f https://raw.githubusercontent.com/kubearmor/KubeArmor/main/pkg/KubeArmorOperator/config/samples/sample-config.yml
```

Проверяем статус установки оператора и сопутствующих сервисов

```bash
kubectl get pods -n kubearmor
```

Пример вывода

```bash
kubearmor-operator-...                    Running
kubearmor-bpf-containerd-...-...          Running
kubearmor-controller-...                  Running
kubearmor-relay-...                       Running
kubearmor-snitch-...                      Completed
```

Проверить, что используется именно BPF-LSM

```bash
DS=$(kubectl get ds -n kubearmor -o name | grep kubearmor | head -1)

kubectl -n kubearmor logs $DS -c kubearmor | grep -iE 'Initialized.*Enforcer|BPF.LSM'
```

Вывод должен быть `Initialized BPF-LSM Enforcer`

### Подготовка образа

Установка зависимостей для сборки PoC

```bash
sudo apt-get install -y docker.io liburing-dev gcc
```

Сборка PoC и образа

```bash
gcc -O2 -o read_secret read_secret.c /usr/lib/x86_64-linux-gnu/liburing.a
sudo docker build -t bad-app:latest -f Dockerfile .
```

Загрузка и проверка образа

```bash
sudo docker save bad-app:latest | sudo k3s ctr images import -

sudo k3s ctr images list | grep bad-app
```

### Дкплой

```bash
kubectl run test-pod-baked \
  --image=bad-app:latest \
  --image-pull-policy=Never \
  --command -- sleep infinity
```

Проверка аннотации для пода

```bash
kubectl get pod test-pod-baked -o yaml | grep -A2 -iE 'kubearmor-policy'
```

Вывод должен быть `kubearmor-policy: enabled`

Применяем политику

```bash
kubectl apply -f policy-block-shadow.yaml
kubectl get kubearmorpolicies -A
```

## Демонстрация

Для `cat`

```bash
kubectl exec test-pod-baked -- cat /etc/shadow
```

Пример вывода

```bash
cat: /etc/shadow: Permission denied command terminated with exit code 1
```

Для `read_secret`

```bash
kubectl exec test-pod-baked -- /usr/local/bin/read_secret
```

Пример вывода

```
=== Bypassing syscall monitoring via io_uring ===
           Failed to open file: /etc/shadow
```

## Очистка кластера

VM в любом случае выкидывается после демо, но если хочется снять
именно KubeArmor:

```bash
kubectl delete -f policy-block-shadow.yaml --ignore-not-found

kubectl delete pod test-pod-baked --ignore-not-found

kubectl delete kubearmorconfig --all -n kubearmor; helm -n kubearmor uninstall kubearmor-operator; kubectl delete ns kubearmor --ignore-not-found; kubectl get crd -o name | grep kubearmor.com | xargs -r kubectl delete
```

Удалить кластер k3s полностью

```bash
sudo /usr/local/bin/k3s-uninstall.sh
```
