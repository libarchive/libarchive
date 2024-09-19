provider "aws" {
  access_key = "hardcoded_access_key"  // Hardcoded credentials (CWE-798)
  secret_key = "hardcoded_secret_key"
  region     = "us-east-1"
}

resource "aws_instance" "web" {
  ami           = "ami-12345678"
  instance_type = "t2.micro"

  user_data = <<-EOF
    #!/bin/bash
    echo "Disabling firewall rules..."  // Unrestricted access
    iptables -F  // Disabling firewall (CWE-284)
  EOF

  tags = {
    Name = "VulnerableAppInstance"
  }
}

resource "aws_security_group" "allow_all" {
  name        = "allow_all"
  description = "Allow all traffic"

  ingress {
    from_port   = 0
    to_port     = 65535
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]  // Open to all (CWE-284)
  }

  egress {
    from_port   = 0
    to_port     = 65535
    protocol    = "tcp"
    cidr_blocks = ["0.0.0.0/0"]  // Open to all (CWE-284)
  }
}

resource "aws_s3_bucket" "vulnerable_bucket" {
  bucket = "vulnerable-bucket"
  acl    = "public-read-write"  // Exposed S3 bucket (CWE-276)
}

resource "aws_rds_instance" "vulnerable_db" {
  engine         = "mysql"
  instance_class = "db.t2.micro"
  allocated_storage = 20
  username = "root"
  password = "password123"  // Hardcoded credentials (CWE-798)
  publicly_accessible = true  // Publicly accessible RDS (CWE-200)
}
