from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from Database.models import Base


# ============================================================
# CONFIGURAÇÃO DO BANCO
# ============================================================

DATABASE_URL = (
    "mysql+pymysql://root:@localhost:3306/cnc_monitor"
)


# ============================================================
# ENGINE
# ============================================================

engine = create_engine(
    DATABASE_URL,
    echo=False,
)


# ============================================================
# SESSÃO
# ============================================================

SessionLocal = sessionmaker(
    bind=engine,
    autocommit=False,
    autoflush=False,
)


# ============================================================
# CRIAÇÃO DAS TABELAS
# ============================================================

def create_tables():
    Base.metadata.create_all(bind=engine)


# ============================================================
# DEPENDÊNCIA DO BANCO
# ============================================================

def get_db():
    db = SessionLocal()

    try:
        yield db
    finally:
        db.close()