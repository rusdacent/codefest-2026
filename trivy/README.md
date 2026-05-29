- [Trivy](#trivy)
  - [Установка](#установка)
  - [Демонстрация](#демонстрация)

# Trivy

## Установка

https://github.com/aquasecurity/trivy#get-trivy

## Демонстрация

```
$ cat test.txt 
PULUMI_TOKEN_TEST="pul-afde034feeebc098ebc4000036e00000deadbeef"
```
```
$ trivy fs --scanners secret test.txt 
```
```
$ cp test.txt a.txt
$ trivy fs --scanners secret a.txt 
```
