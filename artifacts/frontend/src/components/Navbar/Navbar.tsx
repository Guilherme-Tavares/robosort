import { NavLink } from 'react-router-dom';
import './Navbar.css';

export function Navbar() {
  return (
    <header className="navbar">
      <div className="navbar-inner">
        <span className="navbar-brand">Separação Inteligente</span>
        <nav className="navbar-links">
          <NavLink
            to="/dashboard"
            className={({ isActive }) => (isActive ? 'navbar-link navbar-link--active' : 'navbar-link')}
          >
            Dashboard
          </NavLink>
          <NavLink
            to="/compra"
            className={({ isActive }) => (isActive ? 'navbar-link navbar-link--active' : 'navbar-link')}
          >
            Compra
          </NavLink>
          <NavLink
            to="/fila"
            className={({ isActive }) => (isActive ? 'navbar-link navbar-link--active' : 'navbar-link')}
          >
            Fila de separação
          </NavLink>
        </nav>
      </div>
    </header>
  );
}
