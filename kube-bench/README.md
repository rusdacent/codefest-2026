- [kube-bench](#kube-bench)
  - [Деплой](#деплой)
  - [Просмотр отчёта](#просмотр-отчёта)
  - [Очистка кластера](#очистка-кластера)

# kube-bench

## Деплой

Делоим c опцией бенчмарка под k3s

```bash
kubectl apply -f kube-bench-job.yaml
```

```bash
kubectl get pods -A
```

## Просмотр отчёта

```bash
BENCH=$(kubectl get pod -l app=kube-bench -o jsonpath='{.items[0].metadata.name}') ; echo BENCH=$BENCH
```

```bash
kubectl logs $BENCH | grep PKI
```

Пример вывода

```
[FAIL] 1.1.19 Ensure that the Kubernetes PKI directory and file ownership is set to root:root (Automated)
[WARN] 1.1.20 Ensure that the Kubernetes PKI certificate file permissions are set to 600 or more restrictive (Manual)
[FAIL] 1.1.21 Ensure that the Kubernetes PKI key file permissions are set to 600 (Automated)
```

## Очистка кластера

```
kubectl delete -f kube-bench-job.yaml
```
