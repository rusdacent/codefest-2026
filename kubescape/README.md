- [Kubescape](#kubescape)
  - [Установка](#установка)
  - [Демонстрация](#демонстрация)
    - [Генерация отчёта](#генерация-отчёта)
    - [Скан манифеста](#скан-манифеста)

# Kubescape

## Установка

https://github.com/kubescape/kubescape#-installation

## Демонстрация

### Генерация отчёта

```bash
./kubescape scan --format html --output report.html framework mitre
```

### Скан манифеста

Общий отчёт

```
./kubescape scan bad-deployment.yaml
```

Подробный отчёт

```
./kubescape scan control C-0057 bad-deployment.yaml -v
```
